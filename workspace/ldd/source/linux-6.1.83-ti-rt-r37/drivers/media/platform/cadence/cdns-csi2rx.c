// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Cadence MIPI-CSI2 RX Controller v1.3
 *
 * Copyright (C) 2017 Cadence Design Systems Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define CSI2RX_DEVICE_CFG_REG			0x000

#define CSI2RX_SOFT_RESET_REG			0x004
#define CSI2RX_SOFT_RESET_PROTOCOL			BIT(1)
#define CSI2RX_SOFT_RESET_FRONT				BIT(0)

#define CSI2RX_STATIC_CFG_REG			0x008
#define CSI2RX_STATIC_CFG_DLANE_MAP(llane, plane)	((plane) << (16 + (llane) * 4))
#define CSI2RX_STATIC_CFG_LANES_MASK			GENMASK(11, 8)

#define CSI2RX_DPHY_LANE_CTRL_REG		0x40
#define CSI2RX_DPHY_CL_RST			BIT(16)
#define CSI2RX_DPHY_DL_RST(i)			BIT((i) + 12)
#define CSI2RX_DPHY_CL_EN			BIT(4)
#define CSI2RX_DPHY_DL_EN(i)			BIT(i)

#define CSI2RX_STREAM_BASE(n)		(((n) + 1) * 0x100)

#define CSI2RX_STREAM_CTRL_REG(n)		(CSI2RX_STREAM_BASE(n) + 0x000)
#define CSI2RX_STREAM_CTRL_SOFT_RST			BIT(4)
#define CSI2RX_STREAM_CTRL_STOP				BIT(1)
#define CSI2RX_STREAM_CTRL_START			BIT(0)

#define CSI2RX_STREAM_STATUS_REG(n)		(CSI2RX_STREAM_BASE(n) + 0x004)
#define CSI2RX_STREAM_STATUS_RDY			BIT(31)

#define CSI2RX_STREAM_DATA_CFG_REG(n)		(CSI2RX_STREAM_BASE(n) + 0x008)
#define CSI2RX_STREAM_DATA_CFG_VC_SELECT(n)		BIT((n) + 16)
#define CSI2RX_STREAM_DATA_CFG_VC_ALL			0

#define CSI2RX_STREAM_CFG_REG(n)		(CSI2RX_STREAM_BASE(n) + 0x00c)
#define CSI2RX_STREAM_CFG_FIFO_MODE_LARGE_BUF		(1 << 8)

#define CSI2RX_LANES_MAX	4
#define CSI2RX_STREAMS_MAX	4

enum csi2rx_pads {
	CSI2RX_PAD_SINK,
	CSI2RX_PAD_SOURCE_STREAM0,
	CSI2RX_PAD_SOURCE_STREAM1,
	CSI2RX_PAD_SOURCE_STREAM2,
	CSI2RX_PAD_SOURCE_STREAM3,
	CSI2RX_PAD_MAX,
};

struct csi2rx_fmt {
	u32				code;
	u8				bpp;
};

struct csi2rx_priv {
	struct device			*dev;
	unsigned int			count;

	/*
	 * Used to prevent race conditions between multiple,
	 * concurrent calls to start and stop.
	 */
	struct mutex			lock;

	void __iomem			*base;
	struct clk			*sys_clk;
	struct clk			*p_clk;
	struct clk			*pixel_clk[CSI2RX_STREAMS_MAX];
	struct phy			*dphy;

	u8				lanes[CSI2RX_LANES_MAX];
	u8				num_lanes;
	u8				max_lanes;
	u8				max_streams;
	bool				has_internal_dphy;

	struct v4l2_subdev		subdev;
	struct v4l2_async_notifier	notifier;
	struct media_pad		pads[CSI2RX_PAD_MAX];

	/* Remote source */
	struct v4l2_subdev		*source_subdev;
	int				source_pad;
};

static const struct csi2rx_fmt formats[] = {
	{
		.code	= MEDIA_BUS_FMT_YUYV8_1X16,
		.bpp	= 16,
	},
	{
		.code	= MEDIA_BUS_FMT_UYVY8_1X16,
		.bpp	= 16,
	},
	{
		.code	= MEDIA_BUS_FMT_YVYU8_1X16,
		.bpp	= 16,
	},
	{
		.code	= MEDIA_BUS_FMT_VYUY8_1X16,
		.bpp	= 16,
	},
	{
		.code	= MEDIA_BUS_FMT_SBGGR8_1X8,
		.bpp	= 8,
	},
	{
		.code	= MEDIA_BUS_FMT_SGBRG8_1X8,
		.bpp	= 8,
	},
	{
		.code	= MEDIA_BUS_FMT_SGRBG8_1X8,
		.bpp	= 8,
	},
	{
		.code	= MEDIA_BUS_FMT_SRGGB8_1X8,
		.bpp	= 8,
	},
	{
		.code	= MEDIA_BUS_FMT_SBGGR10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SGBRG10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SGRBG10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SRGGB10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SBGGR12_1X12,
		.bpp	= 12,
	},
	{
		.code	= MEDIA_BUS_FMT_SGBRG12_1X12,
		.bpp	= 12,
	},
	{
		.code	= MEDIA_BUS_FMT_SGRBG12_1X12,
		.bpp	= 12,
	},
	{
		.code	= MEDIA_BUS_FMT_SRGGB12_1X12,
		.bpp	= 12,
	},
	{
		.code	= MEDIA_BUS_FMT_SRGGI10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SGRIG10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SBGGI10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SGBIG10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SGIRG10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SIGGR10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SGIBG10_1X10,
		.bpp	= 10,
	},
	{
		.code	= MEDIA_BUS_FMT_SIGGB10_1X10,
		.bpp	= 10,
	},
};

static const struct csi2rx_fmt *csi2rx_get_fmt_by_code(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); i++)
		if (formats[i].code == code)
			return &formats[i];

	return NULL;
}

static int csi2rx_get_frame_desc_from_source(struct csi2rx_priv *csi2rx,
					     struct v4l2_mbus_frame_desc *fd)
{
	struct media_pad *remote_pad;

	remote_pad = media_entity_remote_source_pad_unique(&csi2rx->subdev.entity);
	if (!remote_pad) {
		dev_err(csi2rx->dev, "No remote pad found for sink\n");
		return -ENODEV;
	}

	return v4l2_subdev_call(csi2rx->source_subdev, pad, get_frame_desc,
				remote_pad->index, fd);
}

static inline
struct csi2rx_priv *v4l2_subdev_to_csi2rx(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct csi2rx_priv, subdev);
}

static void csi2rx_reset(struct csi2rx_priv *csi2rx)
{
	unsigned int i;

	writel(CSI2RX_SOFT_RESET_PROTOCOL | CSI2RX_SOFT_RESET_FRONT,
	       csi2rx->base + CSI2RX_SOFT_RESET_REG);

	udelay(10);

	writel(0, csi2rx->base + CSI2RX_SOFT_RESET_REG);

	/* Reset individual streams. */
	for (i = 0; i < csi2rx->max_streams; i++) {
		writel(CSI2RX_STREAM_CTRL_SOFT_RST,
		       csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));
		usleep_range(10, 20);
		writel(0, csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));
	}
}

static int csi2rx_configure_external_dphy(struct csi2rx_priv *csi2rx)
{
	union phy_configure_opts opts = { };
	struct phy_configure_opts_mipi_dphy *cfg = &opts.mipi_dphy;
	struct v4l2_mbus_framefmt *framefmt;
	struct v4l2_subdev_state *state;
	const struct csi2rx_fmt *fmt;
	s64 link_freq;
	int ret;

	state = v4l2_subdev_get_locked_active_state(&csi2rx->subdev);
	framefmt = v4l2_subdev_state_get_stream_format(state, CSI2RX_PAD_SINK, 0);

	if (framefmt) {
		fmt = csi2rx_get_fmt_by_code(framefmt->code);
	} else {
		dev_err(csi2rx->dev, "Did not find active sink format\n");
		fmt = &formats[0];
	}

	link_freq = v4l2_get_link_freq(csi2rx->source_subdev->ctrl_handler,
				       fmt->bpp, 2 * csi2rx->num_lanes);
	if (link_freq < 0)
		return link_freq;

	ret = phy_mipi_dphy_get_default_config_for_hsclk(link_freq,
							 csi2rx->num_lanes, cfg);
	if (ret)
		return ret;

	ret = phy_power_on(csi2rx->dphy);
	if (ret)
		return ret;

	ret = phy_configure(csi2rx->dphy, &opts);
	if (ret)
		phy_power_off(csi2rx->dphy);

	return ret;
}

static int csi2rx_start(struct csi2rx_priv *csi2rx)
{
	unsigned int i;
	unsigned long lanes_used = 0;
	u32 reg;
	int ret;

	ret = clk_prepare_enable(csi2rx->p_clk);
	if (ret)
		return ret;

	csi2rx_reset(csi2rx);

	reg = csi2rx->num_lanes << 8;
	for (i = 0; i < csi2rx->num_lanes; i++) {
		reg |= CSI2RX_STATIC_CFG_DLANE_MAP(i, csi2rx->lanes[i]);
		set_bit(csi2rx->lanes[i], &lanes_used);
	}

	/*
	 * Even the unused lanes need to be mapped. In order to avoid
	 * to map twice to the same physical lane, keep the lanes used
	 * in the previous loop, and only map unused physical lanes to
	 * the rest of our logical lanes.
	 */
	for (i = csi2rx->num_lanes; i < csi2rx->max_lanes; i++) {
		unsigned int idx = find_first_zero_bit(&lanes_used,
						       csi2rx->max_lanes);
		set_bit(idx, &lanes_used);
		reg |= CSI2RX_STATIC_CFG_DLANE_MAP(i, i + 1);
	}

	writel(reg, csi2rx->base + CSI2RX_STATIC_CFG_REG);

	/* Enable DPHY clk and data lanes. */
	if (csi2rx->dphy) {
		reg = CSI2RX_DPHY_CL_EN | CSI2RX_DPHY_CL_RST;
		for (i = 0; i < csi2rx->num_lanes; i++) {
			reg |= CSI2RX_DPHY_DL_EN(csi2rx->lanes[i] - 1);
			reg |= CSI2RX_DPHY_DL_RST(csi2rx->lanes[i] - 1);
		}

		writel(reg, csi2rx->base + CSI2RX_DPHY_LANE_CTRL_REG);

		ret = csi2rx_configure_external_dphy(csi2rx);
		if (ret) {
			dev_err(csi2rx->dev,
				"Failed to configure external DPHY: %d\n", ret);
			goto err_disable_pclk;
		}
	}

	/*
	 * Create a static mapping between the CSI virtual channels
	 * and the output stream.
	 *
	 * This should be enhanced, but v4l2 lacks the support for
	 * changing that mapping dynamically.
	 *
	 * We also cannot enable and disable independent streams here,
	 * hence the reference counting.
	 */
	for (i = 0; i < csi2rx->max_streams; i++) {
		ret = clk_prepare_enable(csi2rx->pixel_clk[i]);
		if (ret)
			goto err_disable_pixclk;

		writel(CSI2RX_STREAM_CFG_FIFO_MODE_LARGE_BUF,
		       csi2rx->base + CSI2RX_STREAM_CFG_REG(i));

		/* Let all virtual channels through. */
		writel(CSI2RX_STREAM_DATA_CFG_VC_ALL,
		       csi2rx->base + CSI2RX_STREAM_DATA_CFG_REG(i));

		writel(CSI2RX_STREAM_CTRL_START,
		       csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));
	}

	ret = clk_prepare_enable(csi2rx->sys_clk);
	if (ret)
		goto err_disable_pixclk;

	clk_disable_unprepare(csi2rx->p_clk);

	return 0;

err_disable_pixclk:
	for (; i > 0; i--)
		clk_disable_unprepare(csi2rx->pixel_clk[i - 1]);

	if (csi2rx->dphy) {
		writel(0, csi2rx->base + CSI2RX_DPHY_LANE_CTRL_REG);
		phy_power_off(csi2rx->dphy);
	}
err_disable_pclk:
	clk_disable_unprepare(csi2rx->p_clk);

	return ret;
}

static void csi2rx_stop(struct csi2rx_priv *csi2rx)
{
	unsigned int i;
	u32 val;
	int ret;

	clk_prepare_enable(csi2rx->p_clk);
	clk_disable_unprepare(csi2rx->sys_clk);

	for (i = 0; i < csi2rx->max_streams; i++) {
		writel(CSI2RX_STREAM_CTRL_STOP,
		       csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));

		ret = readl_relaxed_poll_timeout(csi2rx->base +
						 CSI2RX_STREAM_STATUS_REG(i),
						 val,
						 !(val & CSI2RX_STREAM_STATUS_RDY),
						 10, 10000);
		if (ret)
			dev_warn(csi2rx->dev, "Failed to stop stream%u\n", i);

		clk_disable_unprepare(csi2rx->pixel_clk[i]);
	}

	clk_disable_unprepare(csi2rx->p_clk);

	if (csi2rx->dphy) {
		writel(0, csi2rx->base + CSI2RX_DPHY_LANE_CTRL_REG);

		if (phy_power_off(csi2rx->dphy))
			dev_warn(csi2rx->dev, "Couldn't power off DPHY\n");
	}
}

static int csi2rx_enable_streams(struct v4l2_subdev *subdev,
				 struct v4l2_subdev_state *state, u32 pad,
				 u64 streams_mask)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);
	struct media_pad *remote_pad;
	u64 sink_streams;
	int ret;

	remote_pad = media_pad_remote_pad_first(&csi2rx->pads[CSI2RX_PAD_SINK]);
	if (!remote_pad) {
		dev_err(csi2rx->dev,
			"Failed to find connected source\n");
		return -ENODEV;
	}

	ret = pm_runtime_resume_and_get(csi2rx->dev);
	if (ret < 0)
		return ret;

	sink_streams = v4l2_subdev_state_xlate_streams(state,
						       CSI2RX_PAD_SOURCE_STREAM0,
						       CSI2RX_PAD_SINK,
						       &streams_mask);

	mutex_lock(&csi2rx->lock);
	/*
	 * If we're not the first users, there's no need to
	 * enable the whole controller.
	 */
	if (!csi2rx->count) {
		ret = csi2rx_start(csi2rx);
		if (ret)
			goto err_stream_start;
	}

	/* Start streaming on the source */
	ret = v4l2_subdev_enable_streams(csi2rx->source_subdev, remote_pad->index,
					 sink_streams);
	if (ret) {
		dev_err(csi2rx->dev,
			"Failed to start streams %#llx on subdev\n",
			sink_streams);
		goto err_subdev_enable;
	}

	csi2rx->count++;
	mutex_unlock(&csi2rx->lock);

	return 0;

err_subdev_enable:
	if (!csi2rx->count)
		csi2rx_stop(csi2rx);
err_stream_start:
	mutex_unlock(&csi2rx->lock);
	pm_runtime_put(csi2rx->dev);
	return ret;
}

static int csi2rx_disable_streams(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *state, u32 pad,
				  u64 streams_mask)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);
	struct media_pad *remote_pad;
	u64 sink_streams;

	sink_streams = v4l2_subdev_state_xlate_streams(state,
						       CSI2RX_PAD_SOURCE_STREAM0,
						       CSI2RX_PAD_SINK,
						       &streams_mask);

	remote_pad = media_pad_remote_pad_first(&csi2rx->pads[CSI2RX_PAD_SINK]);
	if (!remote_pad ||
	    v4l2_subdev_disable_streams(csi2rx->source_subdev,
					remote_pad->index, sink_streams)) {
		dev_err(csi2rx->dev, "Couldn't disable our subdev\n");
	}

	mutex_lock(&csi2rx->lock);
	csi2rx->count--;
	/*
	 * Let the last user turn off the lights.
	 */
	if (!csi2rx->count)
		csi2rx_stop(csi2rx);
	mutex_unlock(&csi2rx->lock);

	pm_runtime_put(csi2rx->dev);

	return 0;
}

static int _csi2rx_set_routing(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_state *state,
			       struct v4l2_subdev_krouting *routing)
{
	static const struct v4l2_mbus_framefmt format = {
		.width = 640,
		.height = 480,
		.code = MEDIA_BUS_FMT_UYVY8_1X16,
		.field = V4L2_FIELD_NONE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.ycbcr_enc = V4L2_YCBCR_ENC_601,
		.quantization = V4L2_QUANTIZATION_LIM_RANGE,
		.xfer_func = V4L2_XFER_FUNC_SRGB,
	};
	int ret;

	if (routing->num_routes > V4L2_FRAME_DESC_ENTRY_MAX)
		return -EINVAL;

	ret = v4l2_subdev_routing_validate(subdev, routing,
					   V4L2_SUBDEV_ROUTING_ONLY_1_TO_1);
	if (ret)
		return ret;

	ret = v4l2_subdev_set_routing_with_fmt(subdev, state, routing, &format);
	if (ret)
		return ret;

	return 0;
}

static int csi2rx_set_routing(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_state *state,
			      enum v4l2_subdev_format_whence which,
			      struct v4l2_subdev_krouting *routing)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);

	if (which == V4L2_SUBDEV_FORMAT_ACTIVE && csi2rx->count)
		return -EBUSY;

	return _csi2rx_set_routing(subdev, state, routing);
}

static int csi2rx_set_fmt(struct v4l2_subdev *subdev,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *format)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);
	struct v4l2_mbus_framefmt *fmt;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE && csi2rx->count)
		return -EBUSY;

	/* No transcoding, source and sink formats must match. */
	if (format->pad >= CSI2RX_PAD_SOURCE_STREAM0)
		return v4l2_subdev_get_fmt(subdev, state, format);
	/*
	 * Default to the first format if the requested media bus code isn't
	 * supported.
	 */
	if (!csi2rx_get_fmt_by_code(format->format.code))
		format->format.code = formats[0].code;

	/* Set sink format */
	fmt = v4l2_subdev_state_get_stream_format(state, format->pad,
						  format->stream);
	if (!fmt)
		return -EINVAL;

	*fmt = format->format;

	/* Propagate to source format */
	fmt = v4l2_subdev_state_get_opposite_stream_format(state, format->pad,
							   format->stream);
	if (!fmt)
		return -EINVAL;

	*fmt = format->format;

	return 0;
}

static int csi2rx_init_cfg(struct v4l2_subdev *subdev,
			   struct v4l2_subdev_state *state)
{
	struct v4l2_subdev_route routes[] = {
		{
			.sink_pad = CSI2RX_PAD_SINK,
			.sink_stream = 0,
			.source_pad = CSI2RX_PAD_SOURCE_STREAM0,
			.source_stream = 0,
			.flags = V4L2_SUBDEV_ROUTE_FL_ACTIVE,
		},
	};

	struct v4l2_subdev_krouting routing = {
		.num_routes = ARRAY_SIZE(routes),
		.routes = routes,
	};

	return _csi2rx_set_routing(subdev, state, &routing);
}

static int csi2rx_get_frame_desc(struct v4l2_subdev *subdev, unsigned int pad,
				 struct v4l2_mbus_frame_desc *fd)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);

	return csi2rx_get_frame_desc_from_source(csi2rx, fd);
}

static const struct v4l2_subdev_pad_ops csi2rx_pad_ops = {
	.get_fmt		= v4l2_subdev_get_fmt,
	.set_fmt		= csi2rx_set_fmt,
	.init_cfg		= csi2rx_init_cfg,
	.get_frame_desc		= csi2rx_get_frame_desc,
	.set_routing		= csi2rx_set_routing,
	.enable_streams		= csi2rx_enable_streams,
	.disable_streams	= csi2rx_disable_streams,
};

static const struct v4l2_subdev_ops csi2rx_subdev_ops = {
	.pad		= &csi2rx_pad_ops,
};

static const struct media_entity_operations csi2rx_media_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int csi2rx_async_bound(struct v4l2_async_notifier *notifier,
			      struct v4l2_subdev *s_subdev,
			      struct v4l2_async_subdev *asd)
{
	struct v4l2_subdev *subdev = notifier->sd;
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);

	csi2rx->source_pad = media_entity_get_fwnode_pad(&s_subdev->entity,
							 s_subdev->fwnode,
							 MEDIA_PAD_FL_SOURCE);
	if (csi2rx->source_pad < 0) {
		dev_err(csi2rx->dev, "Couldn't find output pad for subdev %s\n",
			s_subdev->name);
		return csi2rx->source_pad;
	}

	csi2rx->source_subdev = s_subdev;

	dev_dbg(csi2rx->dev, "Bound %s pad: %d\n", s_subdev->name,
		csi2rx->source_pad);

	return media_create_pad_link(&csi2rx->source_subdev->entity,
				     csi2rx->source_pad,
				     &csi2rx->subdev.entity, 0,
				     MEDIA_LNK_FL_ENABLED |
				     MEDIA_LNK_FL_IMMUTABLE);
}

static const struct v4l2_async_notifier_operations csi2rx_notifier_ops = {
	.bound		= csi2rx_async_bound,
};

static int csi2rx_get_resources(struct csi2rx_priv *csi2rx,
				struct platform_device *pdev)
{
	unsigned char i;
	u32 dev_cfg;
	int ret;

	csi2rx->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(csi2rx->base))
		return PTR_ERR(csi2rx->base);

	csi2rx->sys_clk = devm_clk_get(&pdev->dev, "sys_clk");
	if (IS_ERR(csi2rx->sys_clk)) {
		dev_err(&pdev->dev, "Couldn't get sys clock\n");
		return PTR_ERR(csi2rx->sys_clk);
	}

	csi2rx->p_clk = devm_clk_get(&pdev->dev, "p_clk");
	if (IS_ERR(csi2rx->p_clk)) {
		dev_err(&pdev->dev, "Couldn't get P clock\n");
		return PTR_ERR(csi2rx->p_clk);
	}

	csi2rx->dphy = devm_phy_optional_get(&pdev->dev, "dphy");
	if (IS_ERR(csi2rx->dphy)) {
		dev_err(&pdev->dev, "Couldn't get external D-PHY\n");
		return PTR_ERR(csi2rx->dphy);
	}

	ret = clk_prepare_enable(csi2rx->p_clk);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't prepare and enable P clock\n");
		return ret;
	}

	dev_cfg = readl(csi2rx->base + CSI2RX_DEVICE_CFG_REG);
	clk_disable_unprepare(csi2rx->p_clk);

	csi2rx->max_lanes = dev_cfg & 7;
	if (csi2rx->max_lanes > CSI2RX_LANES_MAX) {
		dev_err(&pdev->dev, "Invalid number of lanes: %u\n",
			csi2rx->max_lanes);
		return -EINVAL;
	}

	csi2rx->max_streams = (dev_cfg >> 4) & 7;
	if (csi2rx->max_streams > CSI2RX_STREAMS_MAX) {
		dev_err(&pdev->dev, "Invalid number of streams: %u\n",
			csi2rx->max_streams);
		return -EINVAL;
	}

	csi2rx->has_internal_dphy = dev_cfg & BIT(3) ? true : false;

	/*
	 * FIXME: Once we'll have internal D-PHY support, the check
	 * will need to be removed.
	 */
	if (!csi2rx->dphy && csi2rx->has_internal_dphy) {
		dev_err(&pdev->dev, "Internal D-PHY not supported yet\n");
		return -EINVAL;
	}

	for (i = 0; i < csi2rx->max_streams; i++) {
		char clk_name[16];

		snprintf(clk_name, sizeof(clk_name), "pixel_if%u_clk", i);
		csi2rx->pixel_clk[i] = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(csi2rx->pixel_clk[i])) {
			dev_err(&pdev->dev, "Couldn't get clock %s\n", clk_name);
			return PTR_ERR(csi2rx->pixel_clk[i]);
		}
	}

	return 0;
}

static int csi2rx_parse_dt(struct csi2rx_priv *csi2rx)
{
	struct v4l2_fwnode_endpoint v4l2_ep = { .bus_type = 0 };
	struct v4l2_async_subdev *asd;
	struct fwnode_handle *fwh;
	struct device_node *ep;
	int ret;

	ep = of_graph_get_endpoint_by_regs(csi2rx->dev->of_node, 0, 0);
	if (!ep)
		return -EINVAL;

	fwh = of_fwnode_handle(ep);
	ret = v4l2_fwnode_endpoint_parse(fwh, &v4l2_ep);
	if (ret) {
		dev_err(csi2rx->dev, "Could not parse v4l2 endpoint\n");
		of_node_put(ep);
		return ret;
	}

	if (v4l2_ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(csi2rx->dev, "Unsupported media bus type: 0x%x\n",
			v4l2_ep.bus_type);
		of_node_put(ep);
		return -EINVAL;
	}

	memcpy(csi2rx->lanes, v4l2_ep.bus.mipi_csi2.data_lanes,
	       sizeof(csi2rx->lanes));
	csi2rx->num_lanes = v4l2_ep.bus.mipi_csi2.num_data_lanes;
	if (csi2rx->num_lanes > csi2rx->max_lanes) {
		dev_err(csi2rx->dev, "Unsupported number of data-lanes: %d\n",
			csi2rx->num_lanes);
		of_node_put(ep);
		return -EINVAL;
	}

	v4l2_async_nf_init(&csi2rx->notifier);

	asd = v4l2_async_nf_add_fwnode_remote(&csi2rx->notifier, fwh,
					      struct v4l2_async_subdev);
	of_node_put(ep);
	if (IS_ERR(asd)) {
		v4l2_async_nf_cleanup(&csi2rx->notifier);
		return PTR_ERR(asd);
	}

	csi2rx->notifier.ops = &csi2rx_notifier_ops;

	ret = v4l2_async_subdev_nf_register(&csi2rx->subdev, &csi2rx->notifier);
	if (ret)
		v4l2_async_nf_cleanup(&csi2rx->notifier);

	return ret;
}

static int csi2rx_suspend(struct device *dev)
{
	struct csi2rx_priv *csi2rx = dev_get_drvdata(dev);

	mutex_lock(&csi2rx->lock);
	if (csi2rx->count)
		csi2rx_stop(csi2rx);
	mutex_unlock(&csi2rx->lock);

	return 0;
}

static int csi2rx_resume(struct device *dev)
{
	struct csi2rx_priv *csi2rx = dev_get_drvdata(dev);

	mutex_lock(&csi2rx->lock);
	if (csi2rx->count)
		csi2rx_start(csi2rx);
	mutex_unlock(&csi2rx->lock);
	return 0;
}

static int csi2rx_probe(struct platform_device *pdev)
{
	struct csi2rx_priv *csi2rx;
	unsigned int i;
	int ret;

	csi2rx = kzalloc(sizeof(*csi2rx), GFP_KERNEL);
	if (!csi2rx)
		return -ENOMEM;
	platform_set_drvdata(pdev, csi2rx);
	csi2rx->dev = &pdev->dev;
	mutex_init(&csi2rx->lock);

	ret = csi2rx_get_resources(csi2rx, pdev);
	if (ret)
		goto err_free_priv;

	ret = csi2rx_parse_dt(csi2rx);
	if (ret)
		goto err_free_priv;

	csi2rx->subdev.owner = THIS_MODULE;
	csi2rx->subdev.dev = &pdev->dev;
	v4l2_subdev_init(&csi2rx->subdev, &csi2rx_subdev_ops);
	v4l2_set_subdevdata(&csi2rx->subdev, &pdev->dev);
	snprintf(csi2rx->subdev.name, V4L2_SUBDEV_NAME_SIZE, "%s.%s",
		 KBUILD_MODNAME, dev_name(&pdev->dev));

	/* Create our media pads */
	csi2rx->subdev.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	csi2rx->pads[CSI2RX_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	for (i = CSI2RX_PAD_SOURCE_STREAM0; i < CSI2RX_PAD_MAX; i++)
		csi2rx->pads[i].flags = MEDIA_PAD_FL_SOURCE;
	csi2rx->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		V4L2_SUBDEV_FL_STREAMS;
	csi2rx->subdev.entity.ops = &csi2rx_media_ops;

	ret = media_entity_pads_init(&csi2rx->subdev.entity, CSI2RX_PAD_MAX,
				     csi2rx->pads);
	if (ret)
		goto err_cleanup;

	ret = v4l2_subdev_init_finalize(&csi2rx->subdev);
	if (ret)
		goto err_cleanup;

	pm_runtime_enable(csi2rx->dev);
	ret = v4l2_async_register_subdev(&csi2rx->subdev);
	if (ret < 0)
		goto err_free_subdev;

	dev_info(&pdev->dev,
		 "Probed CSI2RX with %u/%u lanes, %u streams, %s D-PHY\n",
		 csi2rx->num_lanes, csi2rx->max_lanes, csi2rx->max_streams,
		 csi2rx->dphy ? "external" :
		 csi2rx->has_internal_dphy ? "internal" : "no");

	return 0;

err_free_subdev:
	pm_runtime_disable(csi2rx->dev);
	v4l2_subdev_cleanup(&csi2rx->subdev);
err_cleanup:
	v4l2_async_nf_unregister(&csi2rx->notifier);
	v4l2_async_nf_cleanup(&csi2rx->notifier);
	media_entity_cleanup(&csi2rx->subdev.entity);
err_free_priv:
	kfree(csi2rx);
	return ret;
}

static int csi2rx_remove(struct platform_device *pdev)
{
	struct csi2rx_priv *csi2rx = platform_get_drvdata(pdev);

	v4l2_async_nf_unregister(&csi2rx->notifier);
	v4l2_async_nf_cleanup(&csi2rx->notifier);
	v4l2_async_unregister_subdev(&csi2rx->subdev);
	v4l2_subdev_cleanup(&csi2rx->subdev);
	media_entity_cleanup(&csi2rx->subdev.entity);
	pm_runtime_disable(csi2rx->dev);
	kfree(csi2rx);

	return 0;
}

static const struct dev_pm_ops csi2rx_pm_ops = {
	SET_RUNTIME_PM_OPS(csi2rx_suspend, csi2rx_resume, NULL)
};

static const struct of_device_id csi2rx_of_table[] = {
	{ .compatible = "cdns,csi2rx" },
	{ },
};
MODULE_DEVICE_TABLE(of, csi2rx_of_table);

static struct platform_driver csi2rx_driver = {
	.probe	= csi2rx_probe,
	.remove	= csi2rx_remove,

	.driver	= {
		.name		= "cdns-csi2rx",
		.of_match_table	= csi2rx_of_table,
		.pm		= &csi2rx_pm_ops,
	},
};
module_platform_driver(csi2rx_driver);
MODULE_AUTHOR("Maxime Ripard <maxime.ripard@bootlin.com>");
MODULE_DESCRIPTION("Cadence CSI2-RX controller");
MODULE_LICENSE("GPL");

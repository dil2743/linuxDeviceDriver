/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 Texas Instruments Incorporated - https://www.ti.com/
 * Author: Tomi Valkeinen <tomi.valkeinen@ti.com>
 */

#ifndef __TIDSS_DISPC_H__
#define __TIDSS_DISPC_H__

#include "tidss_drv.h"

struct dispc_device;

struct drm_crtc_state;

enum tidss_gamma_type { TIDSS_GAMMA_8BIT, TIDSS_GAMMA_10BIT };

struct tidss_vp_feat {
	struct tidss_vp_color_feat {
		u32 gamma_size;
		enum tidss_gamma_type gamma_type;
		bool has_ctm;
	} color;
};

struct tidss_plane_feat {
	struct tidss_plane_color_feat {
		u32 encodings;
		u32 ranges;
		enum drm_color_encoding default_encoding;
		enum drm_color_range default_range;
	} color;
	struct tidss_plane_blend_feat {
		bool global_alpha;
	} blend;
};

struct dispc_features_scaling {
	u32 in_width_max_5tap_rgb;
	u32 in_width_max_3tap_rgb;
	u32 in_width_max_5tap_yuv;
	u32 in_width_max_3tap_yuv;
	u32 upscale_limit;
	u32 downscale_limit_5tap;
	u32 downscale_limit_3tap;
	u32 xinc_max;
};

struct dispc_errata {
	bool i2000; /* DSS Does Not Support YUV Pixel Data Formats */
};

enum dispc_output_type {
	DISPC_OUTPUT_DPI,		/* DPI output */
	DISPC_OUTPUT_OLDI,		/* OLDI (LVDS) output */
	DISPC_OUTPUT_INTERNAL,		/* SoC internal routing */
	DISPC_OUTPUT_TYPES_MAX,
};

enum dispc_dss_subrevision {
	DISPC_K2G,
	DISPC_AM625,
	DISPC_AM62A7,
	DISPC_AM62P51,
	DISPC_AM62P52,
	DISPC_AM65X,
	DISPC_J721E,
};

enum dispc_oldi_modes {
	OLDI_MODE_SINGLE_LINK,		/* Single output over OLDI 0. */
	OLDI_MODE_CLONE_SINGLE_LINK,	/* Cloned output over OLDI 0 and 1. */
	OLDI_MODE_DUAL_LINK,		/* Combined output over OLDI 0 and 1. */
	OLDI_MODE_OFF,			/* OLDI TXes not connected in OF. */
	OLDI_MODE_UNSUPPORTED,		/* Unsupported OLDI configuration in OF. */
	OLDI_MODE_UNAVAILABLE,		/* OLDI TXes not available in SoC. */
};

struct dispc_features {
	int min_pclk_khz;
	int max_pclk_khz[DISPC_OUTPUT_TYPES_MAX];

	struct dispc_features_scaling scaling;

	enum dispc_dss_subrevision subrev;

	bool has_oldi;

	const char *common;
	const u16 *common_regs;
	u32 num_vps;
	const char *vp_name[TIDSS_MAX_VPS]; /* Should match dt reg names */
	const char *ovr_name[TIDSS_MAX_VPS]; /* Should match dt reg names */
	const char *vpclk_name[TIDSS_MAX_VPS]; /* Should match dt clk names */
	struct tidss_vp_feat vp_feat;
	u32 num_planes;
	const char *vid_name[TIDSS_MAX_PLANES]; /* Should match dt reg names */
	bool vid_lite[TIDSS_MAX_PLANES];
	u32 vid_order[TIDSS_MAX_PLANES];
	u32 num_outputs;
	enum dispc_output_type output_type[TIDSS_MAX_OUTPUTS];
	u32 output_source_vp[TIDSS_MAX_OUTPUTS];
};

extern const struct dispc_features dispc_k2g_feats;
extern const struct dispc_features dispc_am625_feats;
extern const struct dispc_features dispc_am62a7_feats;
extern const struct dispc_features dispc_am62p51_feats;
extern const struct dispc_features dispc_am62p52_feats;
extern const struct dispc_features dispc_am65x_feats;
extern const struct dispc_features dispc_j721e_feats;

enum dispc_output_type dispc_get_output_type(struct dispc_device *dispc,
					     u32 vp_idx);
void dispc_set_irqenable(struct dispc_device *dispc, dispc_irq_t mask);
dispc_irq_t dispc_read_and_clear_irqstatus(struct dispc_device *dispc);

void dispc_ovr_set_plane(struct dispc_device *dispc, u32 hw_plane, u32 vp_idx,
			 u32 x, u32 y, u32 layer);
void dispc_ovr_enable_layer(struct dispc_device *dispc, u32 vp_idx,
			    u32 layer, bool enable);

void dispc_vp_prepare(struct dispc_device *dispc, u32 vp_idx,
		      const struct drm_crtc_state *state);
void dispc_vp_enable(struct dispc_device *dispc, u32 vp_idx,
		     const struct drm_crtc_state *state);
void dispc_vp_disable(struct dispc_device *dispc, u32 vp_idx);
void dispc_vp_unprepare(struct dispc_device *dispc, u32 vp_idx);
bool dispc_vp_go_busy(struct dispc_device *dispc, u32 vp_idx);
void dispc_vp_go(struct dispc_device *dispc, u32 vp_idx);
int dispc_vp_bus_check(struct dispc_device *dispc, u32 vp_idx,
		       const struct drm_crtc_state *state);
enum drm_mode_status dispc_vp_mode_valid(struct dispc_device *dispc, u32 vp_idx,
					 const struct drm_display_mode *mode);
int dispc_vp_enable_clk(struct dispc_device *dispc, u32 vp_idx);
void dispc_vp_disable_clk(struct dispc_device *dispc, u32 vp_idx);
int dispc_vp_set_clk_rate(struct dispc_device *dispc, u32 vp_idx,
			  unsigned long rate);
void dispc_vp_setup(struct dispc_device *dispc, u32 vp_idx,
		    const struct drm_crtc_state *state, bool newmodeset);

int dispc_runtime_suspend(struct dispc_device *dispc);
int dispc_runtime_resume(struct dispc_device *dispc);

int dispc_plane_check(struct dispc_device *dispc, u32 hw_plane,
		      const struct drm_plane_state *state, u32 vp_idx);
int dispc_plane_setup(struct dispc_device *dispc, u32 hw_plane,
		      const struct drm_plane_state *state, u32 vp_idx);
int dispc_plane_enable(struct dispc_device *dispc, u32 hw_plane, bool enable);
const u32 *dispc_plane_formats(struct dispc_device *dispc, unsigned int *len);
void dispc_set_oldi_mode(struct dispc_device *dispc, enum dispc_oldi_modes oldi_mode);

int dispc_init(struct tidss_device *tidss);
void dispc_remove(struct tidss_device *tidss);

void dispc_splash_fini(struct dispc_device *dispc);

#endif

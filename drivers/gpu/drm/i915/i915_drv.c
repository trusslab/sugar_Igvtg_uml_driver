/* i915_drv.c -- i830,i845,i855,i865,i915 driver -*- linux-c -*-
 */
/*
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <linux/device.h>
#include <linux/acpi.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"

#include <linux/console.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <drm/drm_crtc_helper.h>
#include <linux/prints.h>
#include <drm/i915_vgt_isol.h>
#include "../../../pci/pci.h"

bool i915_host_mediate __read_mostly = false;

static struct drm_driver driver;

#define GEN_DEFAULT_PIPEOFFSETS \
	.pipe_offsets = { PIPE_A_OFFSET, PIPE_B_OFFSET, \
			  PIPE_C_OFFSET, PIPE_EDP_OFFSET }, \
	.trans_offsets = { TRANSCODER_A_OFFSET, TRANSCODER_B_OFFSET, \
			   TRANSCODER_C_OFFSET, TRANSCODER_EDP_OFFSET }, \
	.palette_offsets = { PALETTE_A_OFFSET, PALETTE_B_OFFSET }

#define GEN_CHV_PIPEOFFSETS \
	.pipe_offsets = { PIPE_A_OFFSET, PIPE_B_OFFSET, \
			  CHV_PIPE_C_OFFSET }, \
	.trans_offsets = { TRANSCODER_A_OFFSET, TRANSCODER_B_OFFSET, \
			   CHV_TRANSCODER_C_OFFSET, }, \
	.palette_offsets = { PALETTE_A_OFFSET, PALETTE_B_OFFSET, \
			     CHV_PALETTE_C_OFFSET }

#define CURSOR_OFFSETS \
	.cursor_offsets = { CURSOR_A_OFFSET, CURSOR_B_OFFSET, CHV_CURSOR_C_OFFSET }

#define IVB_CURSOR_OFFSETS \
	.cursor_offsets = { CURSOR_A_OFFSET, IVB_CURSOR_B_OFFSET, IVB_CURSOR_C_OFFSET }

static const struct intel_device_info intel_i830_info = {
	.gen = 2, .is_mobile = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_845g_info = {
	.gen = 2, .num_pipes = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i85x_info = {
	.gen = 2, .is_i85x = 1, .is_mobile = 1, .num_pipes = 2,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i865g_info = {
	.gen = 2, .num_pipes = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i915g_info = {
	.gen = 3, .is_i915g = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};
static const struct intel_device_info intel_i915gm_info = {
	.gen = 3, .is_mobile = 1, .num_pipes = 2,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};
static const struct intel_device_info intel_i945g_info = {
	.gen = 3, .has_hotplug = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};
static const struct intel_device_info intel_i945gm_info = {
	.gen = 3, .is_i945gm = 1, .is_mobile = 1, .num_pipes = 2,
	.has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i965g_info = {
	.gen = 4, .is_broadwater = 1, .num_pipes = 2,
	.has_hotplug = 1,
	.has_overlay = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i965gm_info = {
	.gen = 4, .is_crestline = 1, .num_pipes = 2,
	.is_mobile = 1, .has_fbc = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.supports_tv = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_g33_info = {
	.gen = 3, .is_g33 = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_g45_info = {
	.gen = 4, .is_g4x = 1, .need_gfx_hws = 1, .num_pipes = 2,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_gm45_info = {
	.gen = 4, .is_g4x = 1, .num_pipes = 2,
	.is_mobile = 1, .need_gfx_hws = 1, .has_fbc = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.supports_tv = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_pineview_info = {
	.gen = 3, .is_g33 = 1, .is_pineview = 1, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ironlake_d_info = {
	.gen = 5, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ironlake_m_info = {
	.gen = 5, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING,
	.has_llc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_sandybridge_m_info = {
	.gen = 6, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING,
	.has_llc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

#define GEN7_FEATURES  \
	.gen = 7, .num_pipes = 3, \
	.need_gfx_hws = 1, .has_hotplug = 1, \
	.has_fbc = 1, \
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING, \
	.has_llc = 1

static const struct intel_device_info intel_ivybridge_d_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ivybridge_m_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	.is_mobile = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ivybridge_q_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	.num_pipes = 0, /* legal, last one wins */
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_valleyview_m_info = {
	GEN7_FEATURES,
	.is_mobile = 1,
	.num_pipes = 2,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	.has_fbc = 0, /* legal, last one wins */
	.has_llc = 0, /* legal, last one wins */
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_valleyview_d_info = {
	GEN7_FEATURES,
	.num_pipes = 2,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	.has_fbc = 0, /* legal, last one wins */
	.has_llc = 0, /* legal, last one wins */
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_haswell_d_info = {
	GEN7_FEATURES,
	.is_haswell = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_haswell_m_info = {
	GEN7_FEATURES,
	.is_haswell = 1,
	.is_mobile = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_d_info = {
	.gen = 8, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_m_info = {
	.gen = 8, .is_mobile = 1, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_gt3d_info = {
	.gen = 8, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_gt3m_info = {
	.gen = 8, .is_mobile = 1, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_cherryview_info = {
	.gen = 8, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	GEN_CHV_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_skylake_info = {
	.is_skylake = 1,
	.gen = 9, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_skylake_gt3_info = {
	.is_skylake = 1,
	.gen = 9, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broxton_info = {
	.is_preliminary = 1,
	.is_broxton = 1,
	.gen = 9,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.num_pipes = 3,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_kabylake_info = {
	.is_kabylake = 1,
	.gen = 9,
	.num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_kabylake_gt3_info = {
	.is_kabylake = 1,
	.gen = 9,
	.num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

/*
 * Make sure any device matches here are from most specific to most
 * general.  For example, since the Quanta match is based on the subsystem
 * and subvendor IDs, we need it to come before the more general IVB
 * PCI ID matches, otherwise we'll use the wrong info struct above.
 */
#define INTEL_PCI_IDS \
	INTEL_I830_IDS(&intel_i830_info),	\
	INTEL_I845G_IDS(&intel_845g_info),	\
	INTEL_I85X_IDS(&intel_i85x_info),	\
	INTEL_I865G_IDS(&intel_i865g_info),	\
	INTEL_I915G_IDS(&intel_i915g_info),	\
	INTEL_I915GM_IDS(&intel_i915gm_info),	\
	INTEL_I945G_IDS(&intel_i945g_info),	\
	INTEL_I945GM_IDS(&intel_i945gm_info),	\
	INTEL_I965G_IDS(&intel_i965g_info),	\
	INTEL_G33_IDS(&intel_g33_info),		\
	INTEL_I965GM_IDS(&intel_i965gm_info),	\
	INTEL_GM45_IDS(&intel_gm45_info), 	\
	INTEL_G45_IDS(&intel_g45_info), 	\
	INTEL_PINEVIEW_IDS(&intel_pineview_info),	\
	INTEL_IRONLAKE_D_IDS(&intel_ironlake_d_info),	\
	INTEL_IRONLAKE_M_IDS(&intel_ironlake_m_info),	\
	INTEL_SNB_D_IDS(&intel_sandybridge_d_info),	\
	INTEL_SNB_M_IDS(&intel_sandybridge_m_info),	\
	INTEL_IVB_Q_IDS(&intel_ivybridge_q_info), /* must be first IVB */ \
	INTEL_IVB_M_IDS(&intel_ivybridge_m_info),	\
	INTEL_IVB_D_IDS(&intel_ivybridge_d_info),	\
	INTEL_HSW_D_IDS(&intel_haswell_d_info), \
	INTEL_HSW_M_IDS(&intel_haswell_m_info), \
	INTEL_VLV_M_IDS(&intel_valleyview_m_info),	\
	INTEL_VLV_D_IDS(&intel_valleyview_d_info),	\
	INTEL_BDW_GT12M_IDS(&intel_broadwell_m_info),	\
	INTEL_BDW_GT12D_IDS(&intel_broadwell_d_info),	\
	INTEL_BDW_GT3M_IDS(&intel_broadwell_gt3m_info),	\
	INTEL_BDW_GT3D_IDS(&intel_broadwell_gt3d_info), \
	INTEL_CHV_IDS(&intel_cherryview_info),	\
	INTEL_SKL_GT1_IDS(&intel_skylake_info),	\
	INTEL_SKL_GT2_IDS(&intel_skylake_info),	\
	INTEL_SKL_GT3_IDS(&intel_skylake_gt3_info),	\
	INTEL_SKL_GT4_IDS(&intel_skylake_gt3_info), \
	INTEL_BXT_IDS(&intel_broxton_info), \
	INTEL_KBL_GT1_IDS(&intel_kabylake_info), \
	INTEL_KBL_GT2_IDS(&intel_kabylake_info), \
	INTEL_KBL_GT3_IDS(&intel_kabylake_gt3_info), \
	INTEL_KBL_GT4_IDS(&intel_kabylake_gt3_info)

static const struct pci_device_id pciidlist[] = {		/* aka */
	INTEL_PCI_IDS,
	{0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, pciidlist);

static enum intel_pch intel_virt_detect_pch(struct drm_device *dev)
{
	enum intel_pch ret = PCH_NOP;

	/*
	 * In a virtualized passthrough environment we can be in a
	 * setup where the ISA bridge is not able to be passed through.
	 * In this case, a south bridge can be emulated and we have to
	 * make an educated guess as to which PCH is really there.
	 */

	if (IS_GEN5(dev)) {
		ret = PCH_IBX;
		DRM_DEBUG_KMS("Assuming Ibex Peak PCH\n");
	} else if (IS_GEN6(dev) || IS_IVYBRIDGE(dev)) {
		ret = PCH_CPT;
		DRM_DEBUG_KMS("Assuming CouarPoint PCH\n");
	} else if (IS_HASWELL(dev) || IS_BROADWELL(dev)) {
		ret = PCH_LPT;
		DRM_DEBUG_KMS("Assuming LynxPoint PCH\n");
	} else if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev)) {
		ret = PCH_SPT;
		DRM_DEBUG_KMS("Assuming SunrisePoint PCH\n");
	}

	return ret;
}

void intel_detect_pch(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct pci_dev *pch = NULL;

	printk("i915: intel_detect_pch\n");

	/* In all current cases, num_pipes is equivalent to the PCH_NOP setting
	 * (which really amounts to a PCH but no South Display).
	 */
	if (INTEL_INFO(dev)->num_pipes == 0) {
		dev_priv->pch_type = PCH_NOP;
		return;
	}

	/*
	 * The reason to probe ISA bridge instead of Dev31:Fun0 is to
	 * make graphics device passthrough work easy for VMM, that only
	 * need to expose ISA bridge to let driver know the real hardware
	 * underneath. This is a requirement from virtualization team.
	 *
	 * In some virtualized environments (e.g. XEN), there is irrelevant
	 * ISA bridge in the system. To work reliably, we should scan trhough
	 * all the ISA bridge devices and check for the first match, instead
	 * of only checking the first one.
	 */
	while ((pch = pci_get_class(PCI_CLASS_BRIDGE_ISA << 8, pch))) {
		if (pch->vendor == PCI_VENDOR_ID_INTEL) {
			unsigned short id = pch->device & INTEL_PCH_DEVICE_ID_MASK;
			dev_priv->pch_id = id;

			if (id == INTEL_PCH_IBX_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_IBX;
				DRM_DEBUG_KMS("Found Ibex Peak PCH\n");
				WARN_ON(!IS_GEN5(dev));
			} else if (id == INTEL_PCH_CPT_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_CPT;
				DRM_DEBUG_KMS("Found CougarPoint PCH\n");
				WARN_ON(!(IS_GEN6(dev) || IS_IVYBRIDGE(dev)));
			} else if (id == INTEL_PCH_PPT_DEVICE_ID_TYPE) {
				/* PantherPoint is CPT compatible */
				dev_priv->pch_type = PCH_CPT;
				DRM_DEBUG_KMS("Found PantherPoint PCH\n");
				WARN_ON(!(IS_GEN6(dev) || IS_IVYBRIDGE(dev)));
			} else if (id == INTEL_PCH_LPT_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_LPT;
				DRM_DEBUG_KMS("Found LynxPoint PCH\n");
				WARN_ON(!IS_HASWELL(dev) && !IS_BROADWELL(dev));
				WARN_ON(IS_HSW_ULT(dev) || IS_BDW_ULT(dev));
			} else if (id == INTEL_PCH_LPT_LP_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_LPT;
				DRM_DEBUG_KMS("Found LynxPoint LP PCH\n");
				WARN_ON(!IS_HASWELL(dev) && !IS_BROADWELL(dev));
				WARN_ON(!IS_HSW_ULT(dev) && !IS_BDW_ULT(dev));
			} else if (id == INTEL_PCH_SPT_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_SPT;
				DRM_DEBUG_KMS("Found SunrisePoint PCH\n");
				WARN_ON(!IS_SKYLAKE(dev) &&
					!IS_KABYLAKE(dev));
			} else if (id == INTEL_PCH_SPT_LP_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_SPT;
				DRM_DEBUG_KMS("Found SunrisePoint LP PCH\n");
				WARN_ON(!IS_SKYLAKE(dev) &&
					!IS_KABYLAKE(dev));
			} else if (id == INTEL_PCH_P2X_DEVICE_ID_TYPE) {
				dev_priv->pch_type = intel_virt_detect_pch(dev);
			} else
				continue;

			break;
		}
	}
	if (!pch)
		DRM_DEBUG_KMS("No PCH found.\n");

	pci_dev_put(pch);
}

bool i915_semaphore_is_enabled(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		return false;

	if (i915.semaphores >= 0)
		return i915.semaphores;

	/* TODO: make semaphores and Execlists play nicely together */
	if (i915.enable_execlists)
		return false;

	/* Until we get further testing... */
	if (IS_GEN8(dev))
		return false;

#ifdef CONFIG_INTEL_IOMMU
	/* Enable semaphores on SNB when IO remapping is off */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_gfx_mapped)
		return false;
#endif

	return true;
}

void i915_firmware_load_error_print(const char *fw_path, int err)
{
	DRM_ERROR("failed to load firmware %s (%d)\n", fw_path, err);

	/*
	 * If the reason is not known assume -ENOENT since that's the most
	 * usual failure mode.
	 */
	if (!err)
		err = -ENOENT;

	if (!(IS_BUILTIN(CONFIG_DRM_I915) && err == -ENOENT))
		return;

	DRM_ERROR(
	  "The driver is built-in, so to load the firmware you need to\n"
	  "include it either in the kernel (see CONFIG_EXTRA_FIRMWARE) or\n"
	  "in your initrd/initramfs image.\n");
}

static void intel_suspend_encoders(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;

	drm_modeset_lock_all(dev);
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct intel_encoder *intel_encoder = to_intel_encoder(encoder);

		if (intel_encoder->suspend)
			intel_encoder->suspend(intel_encoder);
	}
	drm_modeset_unlock_all(dev);
}

static int intel_suspend_complete(struct drm_i915_private *dev_priv);
static int vlv_resume_prepare(struct drm_i915_private *dev_priv,
			      bool rpm_resume);
static int skl_resume_prepare(struct drm_i915_private *dev_priv);
static int bxt_resume_prepare(struct drm_i915_private *dev_priv);


static int i915_drm_suspend(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	pci_power_t opregion_target_state;
	int error;

	/* ignore lid events during suspend */
	mutex_lock(&dev_priv->modeset_restore_lock);
	dev_priv->modeset_restore = MODESET_SUSPENDED;
	mutex_unlock(&dev_priv->modeset_restore_lock);

	/* We do a lot of poking in a lot of registers, make sure they work
	 * properly. */
	intel_display_set_init_power(dev_priv, true);

	drm_kms_helper_poll_disable(dev);

	pci_save_state(dev->pdev);

	error = i915_gem_suspend(dev);
	if (error) {
		dev_err(&dev->pdev->dev,
			"GEM idle failed, resume might fail\n");
		return error;
	}

	intel_guc_suspend(dev);

	intel_suspend_gt_powersave(dev);

	/*
	 * Disable CRTCs directly since we want to preserve sw state
	 * for _thaw. Also, power gate the CRTC power wells.
	 */
	drm_modeset_lock_all(dev);
	intel_display_suspend(dev);
	drm_modeset_unlock_all(dev);

	intel_dp_mst_suspend(dev);

	intel_runtime_pm_disable_interrupts(dev_priv);
	intel_hpd_cancel_work(dev_priv);

	intel_suspend_encoders(dev_priv);

	intel_suspend_hw(dev);

	i915_gem_suspend_gtt_mappings(dev);

	i915_save_state(dev);

	opregion_target_state = PCI_D3cold;
#if IS_ENABLED(CONFIG_ACPI_SLEEP)
	if (acpi_target_system_state() < ACPI_STATE_S3)
		opregion_target_state = PCI_D1;
#endif
	intel_opregion_notify_adapter(dev, opregion_target_state);

	intel_uncore_forcewake_reset(dev, false);
	intel_opregion_fini(dev);

	intel_fbdev_set_suspend(dev, FBINFO_STATE_SUSPENDED, true);

	dev_priv->suspend_count++;

	intel_display_set_init_power(dev_priv, false);

	return 0;
}

static int i915_drm_suspend_late(struct drm_device *drm_dev, bool hibernation)
{
	struct drm_i915_private *dev_priv = drm_dev->dev_private;
	int ret;

	ret = intel_suspend_complete(dev_priv);

	if (ret) {
		DRM_ERROR("Suspend complete failed: %d\n", ret);

		return ret;
	}

	pci_disable_device(drm_dev->pdev);
	/*
	 * During hibernation on some platforms the BIOS may try to access
	 * the device even though it's already in D3 and hang the machine. So
	 * leave the device in D0 on those platforms and hope the BIOS will
	 * power down the device properly. The issue was seen on multiple old
	 * GENs with different BIOS vendors, so having an explicit blacklist
	 * is inpractical; apply the workaround on everything pre GEN6. The
	 * platforms where the issue was seen:
	 * Lenovo Thinkpad X301, X61s, X60, T60, X41
	 * Fujitsu FSC S7110
	 * Acer Aspire 1830T
	 */
	if (!(hibernation && INTEL_INFO(dev_priv)->gen < 6))
		pci_set_power_state(drm_dev->pdev, PCI_D3hot);

	return 0;
}

int i915_suspend_switcheroo(struct drm_device *dev, pm_message_t state)
{
	int error;

	if (!dev || !dev->dev_private) {
		DRM_ERROR("dev: %p\n", dev);
		DRM_ERROR("DRM not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	if (WARN_ON_ONCE(state.event != PM_EVENT_SUSPEND &&
			 state.event != PM_EVENT_FREEZE))
		return -EINVAL;

	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	error = i915_drm_suspend(dev);
	if (error)
		return error;

	return i915_drm_suspend_late(dev, false);
}

static int i915_drm_resume(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);
	i915_gem_restore_gtt_mappings(dev);
	mutex_unlock(&dev->struct_mutex);

	i915_restore_state(dev);
	intel_opregion_setup(dev);

	intel_init_pch_refclk(dev);
	drm_mode_config_reset(dev);

	/*
	 * Interrupts have to be enabled before any batches are run. If not the
	 * GPU will hang. i915_gem_init_hw() will initiate batches to
	 * update/restore the context.
	 *
	 * Modeset enabling in intel_modeset_init_hw() also needs working
	 * interrupts.
	 */
	intel_runtime_pm_enable_interrupts(dev_priv);

	mutex_lock(&dev->struct_mutex);
	if (i915_gem_init_hw(dev)) {
		DRM_ERROR("failed to re-initialize GPU, declaring wedged!\n");
			atomic_or(I915_WEDGED, &dev_priv->gpu_error.reset_counter);
	}
	mutex_unlock(&dev->struct_mutex);

	intel_guc_resume(dev);

	intel_modeset_init_hw(dev);

	spin_lock_irq(&dev_priv->irq_lock);
	if (dev_priv->display.hpd_irq_setup)
		dev_priv->display.hpd_irq_setup(dev);
	spin_unlock_irq(&dev_priv->irq_lock);

	drm_modeset_lock_all(dev);
	intel_display_resume(dev);
	drm_modeset_unlock_all(dev);

	intel_dp_mst_resume(dev);

	/*
	 * ... but also need to make sure that hotplug processing
	 * doesn't cause havoc. Like in the driver load code we don't
	 * bother with the tiny race here where we might loose hotplug
	 * notifications.
	 * */
	intel_hpd_init(dev_priv);
	/* Config may have changed between suspend and resume */
	drm_helper_hpd_irq_event(dev);

	intel_opregion_init(dev);

	intel_fbdev_set_suspend(dev, FBINFO_STATE_RUNNING, false);

	mutex_lock(&dev_priv->modeset_restore_lock);
	dev_priv->modeset_restore = MODESET_DONE;
	mutex_unlock(&dev_priv->modeset_restore_lock);

	intel_opregion_notify_adapter(dev, PCI_D0);

	drm_kms_helper_poll_enable(dev);

	return 0;
}

static int i915_drm_resume_early(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;

	/*
	 * We have a resume ordering issue with the snd-hda driver also
	 * requiring our device to be power up. Due to the lack of a
	 * parent/child relationship we currently solve this with an early
	 * resume hook.
	 *
	 * FIXME: This should be solved with a special hdmi sink device or
	 * similar so that power domains can be employed.
	 */
	if (pci_enable_device(dev->pdev))
		return -EIO;

	pci_set_master(dev->pdev);

	if (IS_VALLEYVIEW(dev_priv))
		ret = vlv_resume_prepare(dev_priv, false);
	if (ret)
		DRM_ERROR("Resume prepare failed: %d, continuing anyway\n",
			  ret);

	intel_uncore_early_sanitize(dev, true);

	if (IS_BROXTON(dev))
		ret = bxt_resume_prepare(dev_priv);
	else if (IS_SKYLAKE(dev_priv) || IS_KABYLAKE(dev_priv))
		ret = skl_resume_prepare(dev_priv);
	else if (IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv))
		hsw_disable_pc8(dev_priv);

	intel_uncore_sanitize(dev);
	intel_power_domains_init_hw(dev_priv);

	return ret;
}

int i915_resume_switcheroo(struct drm_device *dev)
{
	int ret;

	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	ret = i915_drm_resume_early(dev);
	if (ret)
		return ret;

	return i915_drm_resume(dev);
}

/**
 * i915_reset - reset chip after a hang
 * @dev: drm device to reset
 *
 * Reset the chip.  Useful if a hang is detected. Returns zero on successful
 * reset or otherwise an error code.
 *
 * Procedure is fairly simple:
 *   - reset the chip using the reset reg
 *   - re-init context state
 *   - re-init hardware status page
 *   - re-init ring buffer
 *   - re-init interrupt state
 *   - re-init display
 */
int i915_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool simulated;
	int ret;

	intel_reset_gt_powersave(dev);

	mutex_lock(&dev->struct_mutex);

	i915_gem_reset(dev);

	simulated = dev_priv->gpu_error.stop_rings != 0;

	ret = intel_gpu_reset(dev);

	/* Also reset the gpu hangman. */
	if (simulated) {
		DRM_INFO("Simulated gpu hang, resetting stop_rings\n");
		dev_priv->gpu_error.stop_rings = 0;
		if (ret == -ENODEV) {
			DRM_INFO("Reset not implemented, but ignoring "
				 "error for simulated gpu hangs\n");
			ret = 0;
		}
	}

	if (i915_stop_ring_allow_warn(dev_priv))
		pr_notice("drm/i915: Resetting chip after gpu hang\n");

	if (ret) {
		DRM_ERROR("Failed to reset chip: %i\n", ret);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	intel_overlay_reset(dev_priv);

	/* Ok, now get things going again... */

	/*
	 * Everything depends on having the GTT running, so we need to start
	 * there.  Fortunately we don't need to do this unless we reset the
	 * chip at a PCI level.
	 *
	 * Next we need to restore the context, but we don't use those
	 * yet either...
	 *
	 * Ring buffer needs to be re-initialized in the KMS case, or if X
	 * was running at the time of the reset (i.e. we weren't VT
	 * switched away).
	 */

	/* Used to prevent gem_check_wedged returning -EAGAIN during gpu reset */
	dev_priv->gpu_error.reload_in_reset = true;

	ret = i915_gem_init_hw(dev);

	dev_priv->gpu_error.reload_in_reset = false;

	mutex_unlock(&dev->struct_mutex);
	if (ret) {
		DRM_ERROR("Failed hw init on reset %d\n", ret);
		return ret;
	}

	/*
	 * rps/rc6 re-init is necessary to restore state lost after the
	 * reset and the re-install of gt irqs. Skip for ironlake per
	 * previous concerns that it doesn't respond well to some forms
	 * of re-init after reset.
	 */
	if (INTEL_INFO(dev)->gen > 5)
		intel_enable_gt_powersave(dev);

	return 0;
}

static int i915_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct intel_device_info *intel_info =
		(struct intel_device_info *) ent->driver_data;

	if (IS_PRELIMINARY_HW(intel_info) && !i915.preliminary_hw_support) {
		DRM_INFO("This hardware requires preliminary hardware support.\n"
			 "See CONFIG_DRM_I915_PRELIMINARY_HW_SUPPORT, and/or modparam preliminary_hw_support\n");
		return -ENODEV;
	}

	/* Only bind to function 0 of the device. Early generations
	 * used function 1 as a placeholder for multi-head. This causes
	 * us confusion instead, especially on the systems where both
	 * functions have the same PCI-ID!
	 */
	if (PCI_FUNC(pdev->devfn))
		return -ENODEV;

	driver.driver_features &= ~(DRIVER_USE_AGP);

	return drm_get_pci_dev(pdev, ent, &driver);
}

static void
i915_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_put_dev(dev);
}

static int i915_pm_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int error;

	if (!drm_dev || !drm_dev->dev_private) {
		dev_err(dev, "DRM not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	error = i915_drm_suspend(drm_dev);
	if (error)
		return error;

#ifdef CONFIG_I915_VGT
	if (i915_host_mediate) {
		error = vgt_suspend(pdev);
		if (error)
			return error;
	}
#endif
	return 0;
}

static int i915_pm_suspend_late(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	/*
	 * We have a suspend ordering issue with the snd-hda driver also
	 * requiring our device to be power up. Due to the lack of a
	 * parent/child relationship we currently solve this with an late
	 * suspend hook.
	 *
	 * FIXME: This should be solved with a special hdmi sink device or
	 * similar so that power domains can be employed.
	 */
	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_suspend_late(drm_dev, false);
}

static int i915_pm_poweroff_late(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_suspend_late(drm_dev, true);
}

static int i915_pm_resume_early(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_resume_early(drm_dev);
}

static int i915_pm_resume(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

#ifdef DRM_I915_VGT_SUPPORT
	if (i915_host_mediate) {
		int error = vgt_resume(drm_dev->pdev);
		if (error)
			return error;
	}
#endif

	return i915_drm_resume(drm_dev);
}

static int skl_suspend_complete(struct drm_i915_private *dev_priv)
{
	enum csr_state state;
	/* Enabling DC6 is not a hard requirement to enter runtime D3 */

	skl_uninit_cdclk(dev_priv);

	/* TODO: wait for a completion event or
	 * similar here instead of busy
	 * waiting using wait_for function.
	 */
	wait_for((state = intel_csr_load_status_get(dev_priv)) !=
			FW_UNINITIALIZED, 1000);
	if (state == FW_LOADED)
		skl_enable_dc6(dev_priv);

	return 0;
}

static int hsw_suspend_complete(struct drm_i915_private *dev_priv)
{
	hsw_enable_pc8(dev_priv);

	return 0;
}

static int bxt_suspend_complete(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	/* TODO: when DC5 support is added disable DC5 here. */

	broxton_ddi_phy_uninit(dev);
	broxton_uninit_cdclk(dev);
	bxt_enable_dc9(dev_priv);

	return 0;
}

static int bxt_resume_prepare(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	/* TODO: when CSR FW support is added make sure the FW is loaded */

	bxt_disable_dc9(dev_priv);

	/*
	 * TODO: when DC5 support is added enable DC5 here if the CSR FW
	 * is available.
	 */
	broxton_init_cdclk(dev);
	broxton_ddi_phy_init(dev);
	intel_prepare_ddi(dev);

	return 0;
}

static int skl_resume_prepare(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	if (intel_csr_load_status_get(dev_priv) == FW_LOADED)
		skl_disable_dc6(dev_priv);

	skl_init_cdclk(dev_priv);
	intel_csr_load_program(dev);

	return 0;
}

/*
 * Save all Gunit registers that may be lost after a D3 and a subsequent
 * S0i[R123] transition. The list of registers needing a save/restore is
 * defined in the VLV2_S0IXRegs document. This documents marks all Gunit
 * registers in the following way:
 * - Driver: saved/restored by the driver
 * - Punit : saved/restored by the Punit firmware
 * - No, w/o marking: no need to save/restore, since the register is R/O or
 *                    used internally by the HW in a way that doesn't depend
 *                    keeping the content across a suspend/resume.
 * - Debug : used for debugging
 *
 * We save/restore all registers marked with 'Driver', with the following
 * exceptions:
 * - Registers out of use, including also registers marked with 'Debug'.
 *   These have no effect on the driver's operation, so we don't save/restore
 *   them to reduce the overhead.
 * - Registers that are fully setup by an initialization function called from
 *   the resume path. For example many clock gating and RPS/RC6 registers.
 * - Registers that provide the right functionality with their reset defaults.
 *
 * TODO: Except for registers that based on the above 3 criteria can be safely
 * ignored, we save/restore all others, practically treating the HW context as
 * a black-box for the driver. Further investigation is needed to reduce the
 * saved/restored registers even further, by following the same 3 criteria.
 */
static void vlv_save_gunit_s0ix_state(struct drm_i915_private *dev_priv)
{
	struct vlv_s0ix_state *s = &dev_priv->vlv_s0ix_state;
	int i;

	/* GAM 0x4000-0x4770 */
	s->wr_watermark		= I915_READ(GEN7_WR_WATERMARK);
	s->gfx_prio_ctrl	= I915_READ(GEN7_GFX_PRIO_CTRL);
	s->arb_mode		= I915_READ(ARB_MODE);
	s->gfx_pend_tlb0	= I915_READ(GEN7_GFX_PEND_TLB0);
	s->gfx_pend_tlb1	= I915_READ(GEN7_GFX_PEND_TLB1);

	for (i = 0; i < ARRAY_SIZE(s->lra_limits); i++)
		s->lra_limits[i] = I915_READ(GEN7_LRA_LIMITS(i));

	s->media_max_req_count	= I915_READ(GEN7_MEDIA_MAX_REQ_COUNT);
	s->gfx_max_req_count	= I915_READ(GEN7_GFX_MAX_REQ_COUNT);

	s->render_hwsp		= I915_READ(RENDER_HWS_PGA_GEN7);
	s->ecochk		= I915_READ(GAM_ECOCHK);
	s->bsd_hwsp		= I915_READ(BSD_HWS_PGA_GEN7);
	s->blt_hwsp		= I915_READ(BLT_HWS_PGA_GEN7);

	s->tlb_rd_addr		= I915_READ(GEN7_TLB_RD_ADDR);

	/* MBC 0x9024-0x91D0, 0x8500 */
	s->g3dctl		= I915_READ(VLV_G3DCTL);
	s->gsckgctl		= I915_READ(VLV_GSCKGCTL);
	s->mbctl		= I915_READ(GEN6_MBCTL);

	/* GCP 0x9400-0x9424, 0x8100-0x810C */
	s->ucgctl1		= I915_READ(GEN6_UCGCTL1);
	s->ucgctl3		= I915_READ(GEN6_UCGCTL3);
	s->rcgctl1		= I915_READ(GEN6_RCGCTL1);
	s->rcgctl2		= I915_READ(GEN6_RCGCTL2);
	s->rstctl		= I915_READ(GEN6_RSTCTL);
	s->misccpctl		= I915_READ(GEN7_MISCCPCTL);

	/* GPM 0xA000-0xAA84, 0x8000-0x80FC */
	s->gfxpause		= I915_READ(GEN6_GFXPAUSE);
	s->rpdeuhwtc		= I915_READ(GEN6_RPDEUHWTC);
	s->rpdeuc		= I915_READ(GEN6_RPDEUC);
	s->ecobus		= I915_READ(ECOBUS);
	s->pwrdwnupctl		= I915_READ(VLV_PWRDWNUPCTL);
	s->rp_down_timeout	= I915_READ(GEN6_RP_DOWN_TIMEOUT);
	s->rp_deucsw		= I915_READ(GEN6_RPDEUCSW);
	s->rcubmabdtmr		= I915_READ(GEN6_RCUBMABDTMR);
	s->rcedata		= I915_READ(VLV_RCEDATA);
	s->spare2gh		= I915_READ(VLV_SPAREG2H);

	/* Display CZ domain, 0x4400C-0x4402C, 0x4F000-0x4F11F */
	s->gt_imr		= I915_READ(GTIMR);
	s->gt_ier		= I915_READ(GTIER);
	s->pm_imr		= I915_READ(GEN6_PMIMR);
	s->pm_ier		= I915_READ(GEN6_PMIER);

	for (i = 0; i < ARRAY_SIZE(s->gt_scratch); i++)
		s->gt_scratch[i] = I915_READ(GEN7_GT_SCRATCH(i));

	/* GT SA CZ domain, 0x100000-0x138124 */
	s->tilectl		= I915_READ(TILECTL);
	s->gt_fifoctl		= I915_READ(GTFIFOCTL);
	s->gtlc_wake_ctrl	= I915_READ(VLV_GTLC_WAKE_CTRL);
	s->gtlc_survive		= I915_READ(VLV_GTLC_SURVIVABILITY_REG);
	s->pmwgicz		= I915_READ(VLV_PMWGICZ);

	/* Gunit-Display CZ domain, 0x182028-0x1821CF */
	s->gu_ctl0		= I915_READ(VLV_GU_CTL0);
	s->gu_ctl1		= I915_READ(VLV_GU_CTL1);
	s->pcbr			= I915_READ(VLV_PCBR);
	s->clock_gate_dis2	= I915_READ(VLV_GUNIT_CLOCK_GATE2);

	/*
	 * Not saving any of:
	 * DFT,		0x9800-0x9EC0
	 * SARB,	0xB000-0xB1FC
	 * GAC,		0x5208-0x524C, 0x14000-0x14C000
	 * PCI CFG
	 */
}

static void vlv_restore_gunit_s0ix_state(struct drm_i915_private *dev_priv)
{
	struct vlv_s0ix_state *s = &dev_priv->vlv_s0ix_state;
	u32 val;
	int i;

	/* GAM 0x4000-0x4770 */
	I915_WRITE(GEN7_WR_WATERMARK,	s->wr_watermark);
	I915_WRITE(GEN7_GFX_PRIO_CTRL,	s->gfx_prio_ctrl);
	I915_WRITE(ARB_MODE,		s->arb_mode | (0xffff << 16));
	I915_WRITE(GEN7_GFX_PEND_TLB0,	s->gfx_pend_tlb0);
	I915_WRITE(GEN7_GFX_PEND_TLB1,	s->gfx_pend_tlb1);

	for (i = 0; i < ARRAY_SIZE(s->lra_limits); i++)
		I915_WRITE(GEN7_LRA_LIMITS(i), s->lra_limits[i]);

	I915_WRITE(GEN7_MEDIA_MAX_REQ_COUNT, s->media_max_req_count);
	I915_WRITE(GEN7_GFX_MAX_REQ_COUNT, s->gfx_max_req_count);

	I915_WRITE(RENDER_HWS_PGA_GEN7,	s->render_hwsp);
	I915_WRITE(GAM_ECOCHK,		s->ecochk);
	I915_WRITE(BSD_HWS_PGA_GEN7,	s->bsd_hwsp);
	I915_WRITE(BLT_HWS_PGA_GEN7,	s->blt_hwsp);

	I915_WRITE(GEN7_TLB_RD_ADDR,	s->tlb_rd_addr);

	/* MBC 0x9024-0x91D0, 0x8500 */
	I915_WRITE(VLV_G3DCTL,		s->g3dctl);
	I915_WRITE(VLV_GSCKGCTL,	s->gsckgctl);
	I915_WRITE(GEN6_MBCTL,		s->mbctl);

	/* GCP 0x9400-0x9424, 0x8100-0x810C */
	I915_WRITE(GEN6_UCGCTL1,	s->ucgctl1);
	I915_WRITE(GEN6_UCGCTL3,	s->ucgctl3);
	I915_WRITE(GEN6_RCGCTL1,	s->rcgctl1);
	I915_WRITE(GEN6_RCGCTL2,	s->rcgctl2);
	I915_WRITE(GEN6_RSTCTL,		s->rstctl);
	I915_WRITE(GEN7_MISCCPCTL,	s->misccpctl);

	/* GPM 0xA000-0xAA84, 0x8000-0x80FC */
	I915_WRITE(GEN6_GFXPAUSE,	s->gfxpause);
	I915_WRITE(GEN6_RPDEUHWTC,	s->rpdeuhwtc);
	I915_WRITE(GEN6_RPDEUC,		s->rpdeuc);
	I915_WRITE(ECOBUS,		s->ecobus);
	I915_WRITE(VLV_PWRDWNUPCTL,	s->pwrdwnupctl);
	I915_WRITE(GEN6_RP_DOWN_TIMEOUT,s->rp_down_timeout);
	I915_WRITE(GEN6_RPDEUCSW,	s->rp_deucsw);
	I915_WRITE(GEN6_RCUBMABDTMR,	s->rcubmabdtmr);
	I915_WRITE(VLV_RCEDATA,		s->rcedata);
	I915_WRITE(VLV_SPAREG2H,	s->spare2gh);

	/* Display CZ domain, 0x4400C-0x4402C, 0x4F000-0x4F11F */
	I915_WRITE(GTIMR,		s->gt_imr);
	I915_WRITE(GTIER,		s->gt_ier);
	I915_WRITE(GEN6_PMIMR,		s->pm_imr);
	I915_WRITE(GEN6_PMIER,		s->pm_ier);

	for (i = 0; i < ARRAY_SIZE(s->gt_scratch); i++)
		I915_WRITE(GEN7_GT_SCRATCH(i), s->gt_scratch[i]);

	/* GT SA CZ domain, 0x100000-0x138124 */
	I915_WRITE(TILECTL,			s->tilectl);
	I915_WRITE(GTFIFOCTL,			s->gt_fifoctl);
	/*
	 * Preserve the GT allow wake and GFX force clock bit, they are not
	 * be restored, as they are used to control the s0ix suspend/resume
	 * sequence by the caller.
	 */
	val = I915_READ(VLV_GTLC_WAKE_CTRL);
	val &= VLV_GTLC_ALLOWWAKEREQ;
	val |= s->gtlc_wake_ctrl & ~VLV_GTLC_ALLOWWAKEREQ;
	I915_WRITE(VLV_GTLC_WAKE_CTRL, val);

	val = I915_READ(VLV_GTLC_SURVIVABILITY_REG);
	val &= VLV_GFX_CLK_FORCE_ON_BIT;
	val |= s->gtlc_survive & ~VLV_GFX_CLK_FORCE_ON_BIT;
	I915_WRITE(VLV_GTLC_SURVIVABILITY_REG, val);

	I915_WRITE(VLV_PMWGICZ,			s->pmwgicz);

	/* Gunit-Display CZ domain, 0x182028-0x1821CF */
	I915_WRITE(VLV_GU_CTL0,			s->gu_ctl0);
	I915_WRITE(VLV_GU_CTL1,			s->gu_ctl1);
	I915_WRITE(VLV_PCBR,			s->pcbr);
	I915_WRITE(VLV_GUNIT_CLOCK_GATE2,	s->clock_gate_dis2);
}

int vlv_force_gfx_clock(struct drm_i915_private *dev_priv, bool force_on)
{
	u32 val;
	int err;

#define COND (I915_READ(VLV_GTLC_SURVIVABILITY_REG) & VLV_GFX_CLK_STATUS_BIT)

	val = I915_READ(VLV_GTLC_SURVIVABILITY_REG);
	val &= ~VLV_GFX_CLK_FORCE_ON_BIT;
	if (force_on)
		val |= VLV_GFX_CLK_FORCE_ON_BIT;
	I915_WRITE(VLV_GTLC_SURVIVABILITY_REG, val);

	if (!force_on)
		return 0;

	err = wait_for(COND, 20);
	if (err)
		DRM_ERROR("timeout waiting for GFX clock force-on (%08x)\n",
			  I915_READ(VLV_GTLC_SURVIVABILITY_REG));

	return err;
#undef COND
}

static int vlv_allow_gt_wake(struct drm_i915_private *dev_priv, bool allow)
{
	u32 val;
	int err = 0;

	val = I915_READ(VLV_GTLC_WAKE_CTRL);
	val &= ~VLV_GTLC_ALLOWWAKEREQ;
	if (allow)
		val |= VLV_GTLC_ALLOWWAKEREQ;
	I915_WRITE(VLV_GTLC_WAKE_CTRL, val);
	POSTING_READ(VLV_GTLC_WAKE_CTRL);

#define COND (!!(I915_READ(VLV_GTLC_PW_STATUS) & VLV_GTLC_ALLOWWAKEACK) == \
	      allow)
	err = wait_for(COND, 1);
	if (err)
		DRM_ERROR("timeout disabling GT waking\n");
	return err;
#undef COND
}

static int vlv_wait_for_gt_wells(struct drm_i915_private *dev_priv,
				 bool wait_for_on)
{
	u32 mask;
	u32 val;
	int err;

	mask = VLV_GTLC_PW_MEDIA_STATUS_MASK | VLV_GTLC_PW_RENDER_STATUS_MASK;
	val = wait_for_on ? mask : 0;
#define COND ((I915_READ(VLV_GTLC_PW_STATUS) & mask) == val)
	if (COND)
		return 0;

	DRM_DEBUG_KMS("waiting for GT wells to go %s (%08x)\n",
			wait_for_on ? "on" : "off",
			I915_READ(VLV_GTLC_PW_STATUS));

	/*
	 * RC6 transitioning can be delayed up to 2 msec (see
	 * valleyview_enable_rps), use 3 msec for safety.
	 */
	err = wait_for(COND, 3);
	if (err)
		DRM_ERROR("timeout waiting for GT wells to go %s\n",
			  wait_for_on ? "on" : "off");

	return err;
#undef COND
}

static void vlv_check_no_gt_access(struct drm_i915_private *dev_priv)
{
	if (!(I915_READ(VLV_GTLC_PW_STATUS) & VLV_GTLC_ALLOWWAKEERR))
		return;

	DRM_ERROR("GT register access while GT waking disabled\n");
	I915_WRITE(VLV_GTLC_PW_STATUS, VLV_GTLC_ALLOWWAKEERR);
}

static int vlv_suspend_complete(struct drm_i915_private *dev_priv)
{
	u32 mask;
	int err;

	/*
	 * Bspec defines the following GT well on flags as debug only, so
	 * don't treat them as hard failures.
	 */
	(void)vlv_wait_for_gt_wells(dev_priv, false);

	mask = VLV_GTLC_RENDER_CTX_EXISTS | VLV_GTLC_MEDIA_CTX_EXISTS;
	WARN_ON((I915_READ(VLV_GTLC_WAKE_CTRL) & mask) != mask);

	vlv_check_no_gt_access(dev_priv);

	err = vlv_force_gfx_clock(dev_priv, true);
	if (err)
		goto err1;

	err = vlv_allow_gt_wake(dev_priv, false);
	if (err)
		goto err2;

	if (!IS_CHERRYVIEW(dev_priv->dev))
		vlv_save_gunit_s0ix_state(dev_priv);

	err = vlv_force_gfx_clock(dev_priv, false);
	if (err)
		goto err2;

	return 0;

err2:
	/* For safety always re-enable waking and disable gfx clock forcing */
	vlv_allow_gt_wake(dev_priv, true);
err1:
	vlv_force_gfx_clock(dev_priv, false);

	return err;
}

static int vlv_resume_prepare(struct drm_i915_private *dev_priv,
				bool rpm_resume)
{
	struct drm_device *dev = dev_priv->dev;
	int err;
	int ret;

	/*
	 * If any of the steps fail just try to continue, that's the best we
	 * can do at this point. Return the first error code (which will also
	 * leave RPM permanently disabled).
	 */
	ret = vlv_force_gfx_clock(dev_priv, true);

	if (!IS_CHERRYVIEW(dev_priv->dev))
		vlv_restore_gunit_s0ix_state(dev_priv);

	err = vlv_allow_gt_wake(dev_priv, true);
	if (!ret)
		ret = err;

	err = vlv_force_gfx_clock(dev_priv, false);
	if (!ret)
		ret = err;

	vlv_check_no_gt_access(dev_priv);

	if (rpm_resume) {
		intel_init_clock_gating(dev);
		i915_gem_restore_fences(dev);
	}

	return ret;
}

static int intel_runtime_suspend(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (WARN_ON_ONCE(!(dev_priv->rps.enabled && intel_enable_rc6(dev))))
		return -ENODEV;

	if (WARN_ON_ONCE(!HAS_RUNTIME_PM(dev)))
		return -ENODEV;

	DRM_DEBUG_KMS("Suspending device\n");

	/*
	 * We could deadlock here in case another thread holding struct_mutex
	 * calls RPM suspend concurrently, since the RPM suspend will wait
	 * first for this RPM suspend to finish. In this case the concurrent
	 * RPM resume will be followed by its RPM suspend counterpart. Still
	 * for consistency return -EAGAIN, which will reschedule this suspend.
	 */
	if (!mutex_trylock(&dev->struct_mutex)) {
		DRM_DEBUG_KMS("device lock contention, deffering suspend\n");
		/*
		 * Bump the expiration timestamp, otherwise the suspend won't
		 * be rescheduled.
		 */
		pm_runtime_mark_last_busy(device);

		return -EAGAIN;
	}
	/*
	 * We are safe here against re-faults, since the fault handler takes
	 * an RPM reference.
	 */
	i915_gem_release_all_mmaps(dev_priv);
	mutex_unlock(&dev->struct_mutex);

	intel_guc_suspend(dev);

	intel_suspend_gt_powersave(dev);
	intel_runtime_pm_disable_interrupts(dev_priv);

	ret = intel_suspend_complete(dev_priv);
	if (ret) {
		DRM_ERROR("Runtime suspend failed, disabling it (%d)\n", ret);
		intel_runtime_pm_enable_interrupts(dev_priv);

		return ret;
	}

	cancel_delayed_work_sync(&dev_priv->gpu_error.hangcheck_work);
	intel_uncore_forcewake_reset(dev, false);
	dev_priv->pm.suspended = true;

	/*
	 * FIXME: We really should find a document that references the arguments
	 * used below!
	 */
	if (IS_BROADWELL(dev)) {
		/*
		 * On Broadwell, if we use PCI_D1 the PCH DDI ports will stop
		 * being detected, and the call we do at intel_runtime_resume()
		 * won't be able to restore them. Since PCI_D3hot matches the
		 * actual specification and appears to be working, use it.
		 */
		intel_opregion_notify_adapter(dev, PCI_D3hot);
	} else {
		/*
		 * current versions of firmware which depend on this opregion
		 * notification have repurposed the D1 definition to mean
		 * "runtime suspended" vs. what you would normally expect (D3)
		 * to distinguish it from notifications that might be sent via
		 * the suspend path.
		 */
		intel_opregion_notify_adapter(dev, PCI_D1);
	}

	assert_forcewakes_inactive(dev_priv);

	DRM_DEBUG_KMS("Device suspended\n");
	return 0;
}

static int intel_runtime_resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;

	if (WARN_ON_ONCE(!HAS_RUNTIME_PM(dev)))
		return -ENODEV;

	DRM_DEBUG_KMS("Resuming device\n");

	intel_opregion_notify_adapter(dev, PCI_D0);
	dev_priv->pm.suspended = false;

	intel_guc_resume(dev);

	if (IS_GEN6(dev_priv))
		intel_init_pch_refclk(dev);

	if (IS_BROXTON(dev))
		ret = bxt_resume_prepare(dev_priv);
	else if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev))
		ret = skl_resume_prepare(dev_priv);
	else if (IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv))
		hsw_disable_pc8(dev_priv);
	else if (IS_VALLEYVIEW(dev_priv))
		ret = vlv_resume_prepare(dev_priv, true);

	/*
	 * No point of rolling back things in case of an error, as the best
	 * we can do is to hope that things will still work (and disable RPM).
	 */
	i915_gem_init_swizzling(dev);
	gen6_update_ring_freq(dev);

	intel_runtime_pm_enable_interrupts(dev_priv);

	/*
	 * On VLV/CHV display interrupts are part of the display
	 * power well, so hpd is reinitialized from there. For
	 * everyone else do it here.
	 */
	if (!IS_VALLEYVIEW(dev_priv))
		intel_hpd_init(dev_priv);

	intel_enable_gt_powersave(dev);

	if (ret)
		DRM_ERROR("Runtime resume failed, disabling it (%d)\n", ret);
	else
		DRM_DEBUG_KMS("Device resumed\n");

	return ret;
}

/*
 * This function implements common functionality of runtime and system
 * suspend sequence.
 */
static int intel_suspend_complete(struct drm_i915_private *dev_priv)
{
	int ret;

	if (IS_BROXTON(dev_priv))
		ret = bxt_suspend_complete(dev_priv);
	else if (IS_SKYLAKE(dev_priv) || IS_KABYLAKE(dev_priv))
		ret = skl_suspend_complete(dev_priv);
	else if (IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv))
		ret = hsw_suspend_complete(dev_priv);
	else if (IS_VALLEYVIEW(dev_priv))
		ret = vlv_suspend_complete(dev_priv);
	else
		ret = 0;

	return ret;
}

static const struct dev_pm_ops i915_pm_ops = {
	/*
	 * S0ix (via system suspend) and S3 event handlers [PMSG_SUSPEND,
	 * PMSG_RESUME]
	 */
	.suspend = i915_pm_suspend,
	.suspend_late = i915_pm_suspend_late,
	.resume_early = i915_pm_resume_early,
	.resume = i915_pm_resume,

	/*
	 * S4 event handlers
	 * @freeze, @freeze_late    : called (1) before creating the
	 *                            hibernation image [PMSG_FREEZE] and
	 *                            (2) after rebooting, before restoring
	 *                            the image [PMSG_QUIESCE]
	 * @thaw, @thaw_early       : called (1) after creating the hibernation
	 *                            image, before writing it [PMSG_THAW]
	 *                            and (2) after failing to create or
	 *                            restore the image [PMSG_RECOVER]
	 * @poweroff, @poweroff_late: called after writing the hibernation
	 *                            image, before rebooting [PMSG_HIBERNATE]
	 * @restore, @restore_early : called after rebooting and restoring the
	 *                            hibernation image [PMSG_RESTORE]
	 */
	.freeze = i915_pm_suspend,
	.freeze_late = i915_pm_suspend_late,
	.thaw_early = i915_pm_resume_early,
	.thaw = i915_pm_resume,
	.poweroff = i915_pm_suspend,
	.poweroff_late = i915_pm_poweroff_late,
	.restore_early = i915_pm_resume_early,
	.restore = i915_pm_resume,

	/* S0ix (via runtime suspend) event handlers */
	.runtime_suspend = intel_runtime_suspend,
	.runtime_resume = intel_runtime_resume,
};

static const struct vm_operations_struct i915_gem_vm_ops = {
	.fault = i915_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

const struct file_operations i915_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = i915_compat_ioctl,
#endif
	.llseek = noop_llseek,
};

static struct drm_driver driver = {
	/* Don't use MTRRs here; the Xserver or userspace app should
	 * deal with them for Intel hardware.
	 */
	.driver_features =
	    DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED | DRIVER_GEM | DRIVER_PRIME |
	    DRIVER_RENDER | DRIVER_MODESET,
	.load = i915_driver_load,
	.unload = i915_driver_unload,
	.open = i915_driver_open,
	.lastclose = i915_driver_lastclose,
	.preclose = i915_driver_preclose,
	.postclose = i915_driver_postclose,
	.set_busid = drm_pci_set_busid,

#if defined(CONFIG_DEBUG_FS)
	.debugfs_init = i915_debugfs_init,
	.debugfs_cleanup = i915_debugfs_cleanup,
#endif
	.gem_free_object = i915_gem_free_object,
	.gem_vm_ops = &i915_gem_vm_ops,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = i915_gem_prime_export,
	.gem_prime_import = i915_gem_prime_import,

	.dumb_create = i915_gem_dumb_create,
	.dumb_map_offset = i915_gem_mmap_gtt,
	.dumb_destroy = drm_gem_dumb_destroy,
	.ioctls = i915_ioctls,
	.fops = &i915_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static struct pci_driver i915_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = i915_pci_probe,
	.remove = i915_pci_remove,
	.driver.pm = &i915_pm_ops,
};

static int __init i915_init(void)
{
	driver.num_ioctls = i915_max_ioctl;

	/*
	 * Enable KMS by default, unless explicitly overriden by
	 * either the i915.modeset prarameter or by the
	 * vga_text_mode_force boot option.
	 */

	if (i915.modeset == 0)
		driver.driver_features &= ~DRIVER_MODESET;

#ifdef CONFIG_VGA_CONSOLE
	if (vgacon_text_force() && i915.modeset == -1)
		driver.driver_features &= ~DRIVER_MODESET;
#endif

	if (!(driver.driver_features & DRIVER_MODESET)) {
		/* Silently fail loading to not upset userspace. */
		DRM_DEBUG_DRIVER("KMS and UMS disabled.\n");
		return 0;
	}

	if (i915.nuclear_pageflip)
		driver.driver_features |= DRIVER_ATOMIC;

	return drm_pci_init(&driver, &i915_pci_driver);
}

static void __exit i915_exit(void)
{
	if (!(driver.driver_features & DRIVER_MODESET))
		return; /* Never loaded a driver. */

	drm_pci_exit(&driver, &i915_pci_driver);
}

module_init(i915_init);
module_exit(i915_exit);

#ifdef CONFIG_ISOL_USER

int drm_user_init(void);

static int domainnr = 0; 
static int busnr = 0;

extern struct pci_dev *g_pdev;

/* the same as alloc_pci_dev. */
struct pci_dev *drm_alloc_pci_dev(void)
{
	struct pci_dev *dev;

	dev = kzalloc(sizeof(struct pci_dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	INIT_LIST_HEAD(&dev->bus_list);

	return dev;
}

/* from drivers/pci/probe.c */
static inline unsigned long decode_bar(struct pci_dev *dev, u32 bar)
{
	u32 mem_type;
	unsigned long flags;

	if ((bar & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO) {
		flags = bar & ~PCI_BASE_ADDRESS_IO_MASK;
		flags |= IORESOURCE_IO;
		return flags;
	}

	flags = bar & ~PCI_BASE_ADDRESS_MEM_MASK;
	flags |= IORESOURCE_MEM;
	if (flags & PCI_BASE_ADDRESS_MEM_PREFETCH)
		flags |= IORESOURCE_PREFETCH;

	mem_type = bar & PCI_BASE_ADDRESS_MEM_TYPE_MASK;
	switch (mem_type) {
	case PCI_BASE_ADDRESS_MEM_TYPE_32:
		break;
	case PCI_BASE_ADDRESS_MEM_TYPE_1M:
		/* 1M mem BAR treated as 32-bit BAR */
		break;
	case PCI_BASE_ADDRESS_MEM_TYPE_64:
		flags |= IORESOURCE_MEM_64;
		break;
	default:
		/* mem unknown type treated as 32-bit BAR */
		break;
	}
	return flags;
}

/* from drivers/pci/probe.c */
static u64 pci_size(u64 base, u64 maxbase, u64 mask)
{
	u64 size = mask & maxbase;	/* Find the significant bits */
	if (!size)
		return 0;

	/* Get the lowest of them to find the decode size, and
	   from that the extent.  */
	size = (size & ~(size-1)) - 1;

	/* base == maxbase can be valid only if the BAR has
	   already been programmed with all 1s.  */
	if (base == maxbase && ((base | size) & mask) != mask)
		return 0;

	return size;
}

/* adapted and modified from drivers/pci/probe.c */
#define PCI_COMMAND_DECODE_ENABLE	(PCI_COMMAND_MEMORY | PCI_COMMAND_IO)
int __vgt_isol_pci_read_base(struct pci_dev *dev, enum pci_bar_type type,
		    struct resource *res, unsigned int pos)
{
	u32 l, sz, mask;
	u64 l64, sz64, mask64;
	u16 orig_cmd;
	struct pci_bus_region region, inverted_region;

	mask = type ? PCI_ROM_ADDRESS_MASK : ~0;

	/* No printks while decoding is disabled! */
	if (!dev->mmio_always_on) {
		vgt_isol_pci_read_config_word(dev, PCI_COMMAND, &orig_cmd);
		if (orig_cmd & PCI_COMMAND_DECODE_ENABLE) {
			vgt_isol_pci_write_config_word(dev, PCI_COMMAND,
				orig_cmd & ~PCI_COMMAND_DECODE_ENABLE);
		}
	}

	res->name = pci_name(dev);

	vgt_isol_pci_read_config_dword(dev, pos, &l);
	vgt_isol_pci_write_config_dword(dev, pos, l | mask);
	vgt_isol_pci_read_config_dword(dev, pos, &sz);
	vgt_isol_pci_write_config_dword(dev, pos, l);

	/*
	 * All bits set in sz means the device isn't working properly.
	 * If the BAR isn't implemented, all bits must be 0.  If it's a
	 * memory BAR or a ROM, bit 0 must be clear; if it's an io BAR, bit
	 * 1 must be clear.
	 */
	if (sz == 0xffffffff)
		sz = 0;

	if (l == 0xffffffff)
		l = 0;

	if (type == pci_bar_unknown) {
		res->flags = decode_bar(dev, l);
		res->flags |= IORESOURCE_SIZEALIGN;
		if (res->flags & IORESOURCE_IO) {
			l64 = l & PCI_BASE_ADDRESS_IO_MASK;
			sz64 = sz & PCI_BASE_ADDRESS_IO_MASK;
			mask64 = PCI_BASE_ADDRESS_IO_MASK & (u32)IO_SPACE_LIMIT;
		} else {
			l64 = l & PCI_BASE_ADDRESS_MEM_MASK;
			sz64 = sz & PCI_BASE_ADDRESS_MEM_MASK;
			mask64 = (u32)PCI_BASE_ADDRESS_MEM_MASK;
		}
	} else {
		res->flags |= (l & IORESOURCE_ROM_ENABLE);
		l64 = l & PCI_ROM_ADDRESS_MASK;
		sz64 = sz & PCI_ROM_ADDRESS_MASK;
		mask64 = (u32)PCI_ROM_ADDRESS_MASK;
	}

	if (res->flags & IORESOURCE_MEM_64) {
		vgt_isol_pci_read_config_dword(dev, pos + 4, &l);
		vgt_isol_pci_write_config_dword(dev, pos + 4, ~0);
		vgt_isol_pci_read_config_dword(dev, pos + 4, &sz);
		vgt_isol_pci_write_config_dword(dev, pos + 4, l);

		l64 |= ((u64)l << 32);
		sz64 |= ((u64)sz << 32);
		mask64 |= ((u64)~0 << 32);
	}

	if (!dev->mmio_always_on && (orig_cmd & PCI_COMMAND_DECODE_ENABLE))
		vgt_isol_pci_write_config_word(dev, PCI_COMMAND, orig_cmd);

	if (!sz64)
		goto fail;

	sz64 = pci_size(l64, sz64, mask64);
	if (!sz64) {
		dev_info(&dev->dev, FW_BUG "reg 0x%x: invalid BAR (can't size)\n",
			 pos);
		goto fail;
	}

	if (res->flags & IORESOURCE_MEM_64) {
		if ((sizeof(pci_bus_addr_t) < 8 || sizeof(resource_size_t) < 8)
		    && sz64 > 0x100000000ULL) {
			res->flags |= IORESOURCE_UNSET | IORESOURCE_DISABLED;
			res->start = 0;
			res->end = 0;
			dev_err(&dev->dev, "reg 0x%x: can't handle BAR larger than 4GB (size %#010llx)\n",
				pos, (unsigned long long)sz64);
			goto out;
		}

		if ((sizeof(pci_bus_addr_t) < 8) && l) {
			/* Above 32-bit boundary; try to reallocate */
			res->flags |= IORESOURCE_UNSET;
			res->start = 0;
			res->end = sz64;
			dev_info(&dev->dev, "reg 0x%x: can't handle BAR above 4GB (bus address %#010llx)\n",
				 pos, (unsigned long long)l64);
			goto out;
		}
	}

	region.start = l64;
	region.end = l64 + sz64;

	/*
	 * If "A" is a BAR value (a bus address), "bus_to_resource(A)" is
	 * the corresponding resource address (the physical address used by
	 * the CPU.  Converting that resource address back to a bus address
	 * should yield the original BAR value:
	 *
	 *     resource_to_bus(bus_to_resource(A)) == A
	 *
	 * If it doesn't, CPU accesses to "bus_to_resource(A)" will not
	 * be claimed by the device.
	 */
	if (inverted_region.start != region.start) {
		res->flags |= IORESOURCE_UNSET;
		res->start = 0;
		res->end = region.end - region.start;
		dev_info(&dev->dev, "reg 0x%x: initial BAR value %#010llx invalid\n",
			 pos, (unsigned long long)region.start);
	}

	goto out;

fail:
	res->flags = 0;
out:
	if (res->flags)
		dev_printk(KERN_DEBUG, &dev->dev, "reg 0x%x: %pR\n", pos, res);

	return (res->flags & IORESOURCE_MEM_64) ? 1 : 0;
}

/* adapted and modified from drivers/pci/probe.c */
static void vgt_isol_pci_read_bases(struct pci_dev *dev, unsigned int howmany, int rom)
{
	unsigned int pos, reg;
	unsigned int tmp;

	for (pos = 0; pos < howmany; pos++) {
		struct resource *res = &dev->resource[pos];
		tmp = pos;
		reg = PCI_BASE_ADDRESS_0 + (pos << 2);
		pos += __vgt_isol_pci_read_base(dev, pci_bar_unknown, res, reg);
	}

	if (rom) {
		struct resource *res = &dev->resource[PCI_ROM_RESOURCE];
		dev->rom_base_reg = rom;
		res->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH |
				IORESOURCE_READONLY | IORESOURCE_CACHEABLE |
				IORESOURCE_SIZEALIGN;
		__vgt_isol_pci_read_base(dev, pci_bar_mem32, res, rom);
	}
}

/* adapted and modified from drivers/pci/probe.c */
static bool vgt_isol_pci_ext_cfg_is_aliased(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_QUIRKS
	int pos;
	u32 header, tmp;

	vgt_isol_pci_read_config_dword(dev, PCI_VENDOR_ID, &header);

	for (pos = PCI_CFG_SPACE_SIZE;
	     pos < PCI_CFG_SPACE_EXP_SIZE; pos += PCI_CFG_SPACE_SIZE) {
		if (vgt_isol_pci_read_config_dword(dev, pos, &tmp) != PCIBIOS_SUCCESSFUL
		    || header != tmp)
			return false;
	}

	return true;
#else
	return false;
#endif
}

/* adapted and modified from drivers/pci/probe.c */
static int vgt_isol_pci_cfg_space_size_ext(struct pci_dev *dev)
{
	u32 status;
	int pos = PCI_CFG_SPACE_SIZE;

	if (pci_read_config_dword(dev, pos, &status) != PCIBIOS_SUCCESSFUL)
		goto fail;
	if (status == 0xffffffff || vgt_isol_pci_ext_cfg_is_aliased(dev))
		goto fail;

	return PCI_CFG_SPACE_EXP_SIZE;

 fail:
	return PCI_CFG_SPACE_SIZE;
}

/* adapted and modified from drivers/pci/probe.c */
int vgt_isol_pci_cfg_space_size(struct pci_dev *dev)
{
	int pos;
	u32 status;
	u16 class;

	class = dev->class >> 8;
	if (class == PCI_CLASS_BRIDGE_HOST)
		BUG();

	if (!pci_is_pcie(dev)) {
		BUG();

	}

	return vgt_isol_pci_cfg_space_size_ext(dev);

 fail:
	return PCI_CFG_SPACE_SIZE;
}

/* adapted and modified from drivers/pci/probe.c */
int vgt_isol_pci_setup_device(struct pci_dev *dev)
{
	u32 class;
	u8 hdr_type;
	struct pci_slot *slot;
	int pos = 0;
	struct pci_bus_region region;
	struct resource *res;

	if (vgt_isol_pci_read_config_byte(dev, PCI_HEADER_TYPE, &hdr_type))
		return -EIO;

	dev->hdr_type = hdr_type & 0x7f;
	dev->multifunction = !!(hdr_type & 0x80);
	dev->error_state = pci_channel_io_normal;

	/* Assume 32-bit PCI; let 64-bit PCI cards (which are far rarer)
	   set this higher, assuming the system even supports it.  */
	dev->dma_mask = 0xffffffff;

	dev_set_name(&dev->dev, "%04x:%02x:%02x.%d", domainnr,
		     busnr, PCI_SLOT(dev->devfn),
		     PCI_FUNC(dev->devfn));

	vgt_isol_pci_read_config_dword(dev, PCI_CLASS_REVISION, &class);
	dev->revision = class & 0xff;
	dev->class = class >> 8;		    /* upper 3 bytes */

	dev_printk(KERN_DEBUG, &dev->dev, "[%04x:%04x] type %02x class %#08x\n",
		   dev->vendor, dev->device, dev->hdr_type, dev->class);

	/* need to have dev->class ready */
	dev->cfg_size = vgt_isol_pci_cfg_space_size(dev);

	/* "Unknown power state" */
	dev->current_state = PCI_UNKNOWN;

	/* Early fixups, before probing the BARs */
	/* device class may be changed after fixup */
	class = dev->class >> 8;

	switch (dev->hdr_type) {		    /* header type */
	case PCI_HEADER_TYPE_NORMAL:		    /* standard header */
		if (class == PCI_CLASS_BRIDGE_PCI)
			goto bad;
		vgt_isol_pci_read_bases(dev, 6, PCI_ROM_ADDRESS);
		vgt_isol_pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, &dev->subsystem_vendor);
		vgt_isol_pci_read_config_word(dev, PCI_SUBSYSTEM_ID, &dev->subsystem_device);

		/*
		 * Do the ugly legacy mode stuff here rather than broken chip
		 * quirk code. Legacy mode ATA controllers have fixed
		 * addresses. These are not always echoed in BAR0-3, and
		 * BAR0-3 in a few cases contain junk!
		 */
		if (class == PCI_CLASS_STORAGE_IDE) {
			BUG();
		}
		break;

	case PCI_HEADER_TYPE_BRIDGE:		    /* bridge header */
		BUG();
		break;

	case PCI_HEADER_TYPE_CARDBUS:		    /* CardBus bridge header */
		BUG();
		break;

	default:				    /* unknown header */
		dev_err(&dev->dev, "unknown header type %02x, ignoring device\n",
			dev->hdr_type);
		return -EIO;

	bad:
		dev_err(&dev->dev, "ignoring class %#08x (doesn't match header type %02x)\n",
			dev->class, dev->hdr_type);
		dev->class = PCI_CLASS_NOT_DEFINED;
	}

	vgt_isol_pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_MEMORY);

	return 0;
}

int __init i915_user_init(void)
{
	char pci_device_name[32];
	struct pci_dev *pdev;
	struct pci_device_id *pdevid;
	unsigned char dev_node_name[32];
	unsigned int devfn;
	unsigned short vendor;
	unsigned short device;
	unsigned short subsystem_vendor;
	unsigned short subsystem_device;
	unsigned int class;
	int cfg_size;
	char driver_name[32];
	u8 *config;
	unsigned long *resources;

	drm_user_init();
	
	i915_init();
	
	devfn = 0x20; /* device number = 2, function number = 0 */
	vendor = 0x8086;
	device = 0x1622;
	
	pdev = drm_alloc_pci_dev();
	if (!pdev)
		return -ENOMEM;

	pdev->dev.bus = NULL;
	pdev->dev.parent = NULL;
	
	pdev->bus = NULL; 

	pdev->devfn = devfn;
	pdev->vendor = vendor;
	pdev->pcie_cap = 1;
	pdev->mmio_always_on = 1;

	if (vgt_isol_pci_setup_device(pdev))
		return -EFAULT;

	pdev->device = device;
	pdev->driver = NULL;
	
	pdevid = (struct pci_device_id *) kmalloc(sizeof(struct pci_device_id),
								GFP_KERNEL);
	pdevid->vendor = pdev->vendor;
	pdevid->device = pdev->device;
	pdevid->subvendor = pdev->subsystem_vendor;
	pdevid->subdevice = pdev->subsystem_device;
	pdevid->class = pdev->class;
	pdevid->driver_data = &intel_broadwell_gt3m_info; 
	
	g_pdev = pdev;

	vgt_isol_pci_write_config_word(pdev, 0x92, 0x1);
	
	i915_pci_probe(pdev, pdevid);
	
	return 0;
}

#endif /* CONFIG_ISOL_USER */

MODULE_AUTHOR("Tungsten Graphics, Inc.");
MODULE_AUTHOR("Intel Corporation");

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");

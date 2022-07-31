// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan_base.h"

#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {

VSamplerSetup::VSamplerSetup(VTexFilter mag_filter, VTexFilter min_filter,
							 Maybe<VTexFilter> mip_filter, VTexAddress address_mode,
							 uint max_anisotropy_samples)
	: mag_filter(mag_filter), min_filter(min_filter),
	  mipmap_filter(mip_filter), address_mode{address_mode, address_mode, address_mode},
	  max_anisotropy_samples(max_anisotropy_samples) {}

VSamplerSetup::VSamplerSetup(VTexFilter mag_filter, VTexFilter min_filter,
							 Maybe<VTexFilter> mip_filter, array<VTexAddress, 3> address_mode,
							 uint max_anisotropy_samples)
	: mag_filter(mag_filter), min_filter(min_filter), mipmap_filter(mip_filter),
	  address_mode(address_mode), max_anisotropy_samples(max_anisotropy_samples) {}

int primitiveCount(VPrimitiveTopology topo, int vertex_count) {
	using Topo = VPrimitiveTopology;
	switch(topo) {
	case Topo::point_list:
		return vertex_count;
	case Topo::line_list:
		return vertex_count / 2;
	case Topo::line_strip:
		return max(0, vertex_count - 1);
	case Topo::triangle_list:
		return vertex_count / 3;
	case Topo::triangle_strip:
	case Topo::triangle_fan:
		return max(0, vertex_count - 2);
	}
	return 0;
}

}
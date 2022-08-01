// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_buffer.h"

namespace fwk {

template <class T> struct VBufferSpan {
	VBufferSpan(PVBuffer buffer)
		: m_buffer(buffer), m_byte_offset(0), m_size(buffer ? buffer->size() / sizeof(T) : 0) {}
	VBufferSpan(PVBuffer buffer, u32 byte_offset) : m_buffer(buffer), m_byte_offset(byte_offset) {
		auto buffer_byte_size = buffer ? buffer->size() : 0;
		DASSERT_LE(byte_offset, buffer_size);
		size = (buffer_byte_size - byte_offset) / sizeof(T);
	}
	VBufferSpan(PVBuffer buffer, u32 byte_offset, u32 size)
		: m_buffer(buffer), m_byte_offset(byte_offset), m_size(size) {
		auto buffer_byte_size = buffer ? buffer->size() : 0;
		DASSERT_LE(m_byte_offset + m_size * sizeof(T), buffer_byte_size);
	}
	VBufferSpan(PVBuffer buffer, u32 byte_offset, u32 size, NoAssertsTag)
		: m_buffer(buffer), m_byte_offset(byte_offset), m_size(size) {}
	VBufferSpan() : m_byte_offset(0), m_size(0) {}

	PVBuffer buffer() const { return m_buffer; }
	u32 size() const { return m_size; }
	u32 byteOffset() const { return m_byte_offset; }
	u32 byteSize() const { return m_size * sizeof(T); }
	u32 byteEndOffset() const { return m_byte_offset + byteSize(); }
	explicit operator bool() const { return m_size != 0; }

	VBufferSpan operator+(int offset) const {
		DASSERT_GE(m_byte_offset + offset * sizeof(T), 0);
		DASSERT_LE(offset, m_size);
		return VBufferSpan(m_buffer, u32(m_byte_offset + offset * sizeof(T)), u32(m_size - offset),
						   no_asserts);
	}
	VBufferSpan operator-(int offset) const { return operator+(-offset); }

	VBufferSpan subSpan(uint start) const {
		DASSERT_LE(start, m_size);
		return {m_buffer, u32(m_byte_offset + start * sizeof(T)), m_size - start};
	}
	VBufferSpan subSpan(uint start, uint end) const {
		DASSERT_LE(start, end);
		DASSERT_LE(end, m_size);
		return {m_buffer, u32(m_byte_offset + start * sizeof(T)), end - start};
	}

	template <class U> auto reinterpret() const {
		static_assert(compatibleSizes(sizeof(T), sizeof(U)),
					  "Incompatible sizes; are you sure, you want to do this cast?");
		using out_type = If<fwk::is_const<T>, const U, U>;
		auto new_size = size_t(m_size) * sizeof(T) / sizeof(U);
		return VBufferSpan<out_type>(m_buffer, m_byte_offset, new_size);
	}

  private:
	PVBuffer m_buffer;
	u32 m_byte_offset, m_size;
};

template <class T> VBufferSpan<T> span(PVBuffer buffer, u32 byte_offset, u32 size) {
	return VBufferSpan<T>(buffer, byte_offset, size);
}

inline VBufferSpan<char> span(PVBuffer buffer, u32 byte_offset, u32 size) {
	return VBufferSpan<char>(buffer, byte_offset, size);
}

template <class T>
Ex<VBufferSpan<T>> VulkanBuffer::create(VulkanDevice &device, u32 num_elements,
										VBufferUsageFlags usage, VMemoryUsage mem_usage) {
	if(!num_elements)
		return VBufferSpan<T>();
	auto buffer = EX_PASS(create(device, num_elements * sizeof(T), usage, mem_usage));
	return VBufferSpan<T>(buffer);
}

template <c_span TSpan, class T>
Ex<VBufferSpan<T>> VulkanBuffer::createAndUpload(VulkanDevice &device, const TSpan &data,
												 VBufferUsageFlags usage, VMemoryUsage mem_usage) {
	if(!data)
		return VBufferSpan<T>();
	auto buffer = EX_PASS(
		createAndUpload(device, cspan(data).template reinterpret<char>(), usage, mem_usage));
	return VBufferSpan<T>(buffer);
}

}
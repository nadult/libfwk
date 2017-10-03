// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/matrix_stack.h"

namespace fwk {

MatrixStack::MatrixStack(const Matrix4 &proj_matrix, const Matrix4 &view_matrix)
	: m_projection_matrix(proj_matrix), m_view_matrix(view_matrix), m_is_dirty(true),
	  m_is_frustum_dirty(true) {}

void MatrixStack::pushViewMatrix() { m_matrix_stack.push_back(m_view_matrix); }

void MatrixStack::popViewMatrix() {
	DASSERT(!m_matrix_stack.empty());
	m_view_matrix = m_matrix_stack.back();
	m_matrix_stack.pop_back();
	m_is_dirty = m_is_frustum_dirty = true;
}

void MatrixStack::mulViewMatrix(const Matrix4 &matrix) {
	m_view_matrix = m_view_matrix * matrix;
	m_is_dirty = m_is_frustum_dirty = true;
}

void MatrixStack::setViewMatrix(const Matrix4 &matrix) {
	m_view_matrix = matrix;
	m_is_dirty = m_is_frustum_dirty = true;
}

const Matrix4 &MatrixStack::fullMatrix() const {
	if(m_is_dirty) {
		m_full_matrix = m_projection_matrix * m_view_matrix;
		m_is_dirty = false;
	}
	return m_full_matrix;
}

const Frustum MatrixStack::frustum() const {
	if(m_is_frustum_dirty) {
		m_frustum = Frustum(fullMatrix());
		m_is_frustum_dirty = false;
	}
	return m_frustum;
}
}

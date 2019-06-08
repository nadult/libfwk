// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/perf/exec_tree.h"

#include "fwk/math/hash.h"
#include "fwk/sys/assert.h"

namespace perf {

ExecNodeType toNode(SampleType type) {
	switch(type) {
	case SampleType::scope_begin:
	case SampleType::scope_end:
		return ExecNodeType::scope;
	case SampleType::counter:
		return ExecNodeType::counter;
	case SampleType::gpu_time:
		return ExecNodeType::gpu_time;
	}
	return {};
}

ExecTree::ExecTree(int reserve) {
	nodes.reserve(reserve);
	nodes.emplace_back(PointId(0), NodeType::scope, ExecId(0), 0);
	m_stack.reserve(64);
}

ExecId ExecTree::get(ExecId parent_id, PointId point_id, NodeType type) {
	DASSERT(point_id);

	auto &parent = nodes[parent_id];
	for(auto child_id : parent.children)
		if(nodes[child_id].point_id == point_id && nodes[child_id].type == type)
			return child_id;

	DASSERT_EQ(parent.type, NodeType::scope);
	DASSERT_LT(nodes.size(), max_size);

	ExecId id(nodes.size());
	DASSERT(pointInfo(point_id));
	parent.children.emplace_back(id);
	if(type == NodeType::gpu_time) {
		PASSERT(!parent.gpu_time_id && "Each scope can have at most 1 gpu_time sub-node");
		PASSERT(point_id == parent.point_id);
		parent.gpu_time_id = id;
	}

	nodes.emplace_back(point_id, type, parent_id, parent.depth + 1);
	m_descendants_outdated = true;
	return id;
}

vector<ExecId> ExecTree::mapSamples(CSpan<PSample> samples) {
	vector<ExecId> out;
	out.reserve(samples.size());

	m_stack.clear();
	m_stack.emplace_back(root());

	for(auto &sample : samples) {
		auto parent_id = m_stack.back();
		ExecId node_id;

		switch(sample.type()) {
		case SampleType::scope_begin:
			node_id = get(parent_id, sample.id(), NodeType::scope);
			out.emplace_back(node_id);
			m_stack.emplace_back(node_id);
			break;
		case SampleType::scope_end:
			DASSERT(nodes[parent_id].point_id == sample.id());
			out.emplace_back(parent_id);
			m_stack.pop_back();
			break;
		case SampleType::counter:
		case SampleType::gpu_time:
			node_id = get(parent_id, sample.id(), toNode(sample.type()));
			out.emplace_back(node_id);
			break;
		}
	}

	return out;
}

vector<ESample> ExecTree::makeExecSamples(vector<PSample> samples) {
	m_stack.clear();
	m_stack.emplace_back(root());

	for(auto &sample : samples) {
		auto parent_id = m_stack.back();
		ExecId node_id;

		switch(sample.type()) {
		case SampleType::scope_begin:
			node_id = get(parent_id, sample.id(), NodeType::scope);
			m_stack.emplace_back(node_id);
			break;
		case SampleType::scope_end:
			node_id = parent_id;
			m_stack.pop_back();
			break;
		case SampleType::counter:
		case SampleType::gpu_time:
			node_id = get(parent_id, sample.id(), toNode(sample.type()));
			break;
		}

		sample = {sample.type(), PointId(node_id), sample.value()};
	}

	return samples.reinterpret<ESample>();
}

bool ExecTree::emptyBranches(ExecId exec_id, Span<bool> out, CSpan<i64> values) const {
	bool result = values[exec_id] == 0;

	for(auto child_id : nodes[exec_id].children)
		if(child_id < values.size())
			result = emptyBranches(child_id, out, values) && result;

	out[exec_id] = result;
	return result;
}

u64 ExecTree::hashedId(ExecId exec_id) const {
	if(exec_id == root())
		return 0;

	auto &exec = nodes[exec_id];
	auto *point = pointInfo(exec.point_id);
	return hashMany<u64>(hashedId(exec.parent_id), point->func.name, point->func.args);
}

vector<bool> ExecTree::emptyBranches(CSpan<i64> values) const {
	vector<bool> out(nodes.size(), false);
	if(values)
		emptyBranches(root(), out, values);
	return out;
}

vector<PSample> ExecTree::makePointSamples(vector<ESample> samples) const {
	for(auto &sample : samples) {
		PASSERT(sample.id() != root());
		sample = {sample.type(), ExecId(get(sample.id()).point_id), sample.value()};
	}
	return samples.reinterpret<PSample>();
}

void ExecTree::getExecValues(CSpan<ESample> samples, vector<ExecValue> &values) const {
	DASSERT(values.size() == size());
	for(auto &sample : samples) {
		auto value = sample.value();
		bool scope_begin = sample.type() == SampleType::scope_begin;
		auto type = sample.type();
		auto &out = values[sample.id()];
		out.value += scope_begin ? -i64(value) : i64(value);
		if(scope_begin) {
			out.begin_time += value;
			out.num_instances++;
		}
	}
}

void ExecTree::scaleCpuTimes(Span<ESample> samples, double scale) const {
	for(auto &sample : samples)
		if(isOneOf(sample.type(), SampleType::scope_begin, SampleType::scope_end))
			sample.setValue(double(sample.value()) * scale);
}

pair<u64, int> ExecTree::computeGpuTimes(int sample_id, Span<ESample> samples) const {
	auto &sample = samples[sample_id];
	if(sample.type() == SampleType::scope_begin) {
		ESample *gpu_sample = nullptr;
		if(sample_id + 1 < samples.size() && samples[sample_id + 1].type() == SampleType::gpu_time)
			gpu_sample = &samples[sample_id + 1];

		u64 sum = 0;
		for(int n = sample_id + 1; n < samples.size(); n++) {
			auto &next_sample = samples[n];
			if(next_sample.type() == SampleType::scope_begin) {
				auto result = computeGpuTimes(n, samples);
				sum += result.first;
				n = result.second;
			} else if(next_sample.type() == SampleType::scope_end) {
				sample_id = n;
				break;
			}
		}

		if(gpu_sample) {
			sum += gpu_sample->value();
			gpu_sample->setValue(sum);
		}
		return {sum, sample_id};
	}

	return {0, sample_id};
}

void ExecTree::computeGpuTimes(Span<ESample> samples) {
	// When gathered, gpu time values exclude times from child scopes
	// This function compute total gpu execution time for all scopes

	for(int n = 0; n < samples.size(); n++) {
		auto result = computeGpuTimes(n, samples);
		n = result.second;
	}
}

int ExecTree::depth(ExecId id) const {
	int out = 1;
	while(id != root()) {
		id = nodes[id].parent_id;
		out++;
	}

	return out;
}

void ExecTree::updateDescendants() {}

int ExecTree::usedMemory() const {
	int out = 0;
	for(auto &exec : nodes)
		if(!exec.children.isSmall())
			out += exec.children.usedMemory();
	return out + nodes.usedMemory();
}
}

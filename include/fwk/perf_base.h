// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"
#include "fwk/vector.h"

namespace perf {
using namespace fwk;

// Naming:
// point: identifies a point in code from which a sample was generated
// execution: identifies a stack of scopes, each identified with a point

struct PointId {
	static constexpr int max = 65535;

	explicit PointId(unsigned short v) : value(v) {}
	PointId() : value(0) {}

	explicit operator bool() const { return value != 0; }
	operator unsigned short() const { return value; }

	bool operator==(const PointId &rhs) const { return value == rhs.value; }
	bool operator<(const PointId &rhs) const { return value == rhs.value; }

	unsigned short value;
};

struct ExecId {
	static constexpr int max = 65535;

	explicit ExecId(unsigned short v) : value(v) {}
	ExecId() : value(0) {}

	explicit operator bool() const { return value != 0; }
	operator unsigned short() const { return value; }

	bool operator==(const ExecId &rhs) const { return value == rhs.value; }
	bool operator<(const ExecId &rhs) const { return value == rhs.value; }

	unsigned short value;
};

DEFINE_ENUM(SampleType, scope_begin, scope_end, gpu_time, counter);
DEFINE_ENUM(PointType, scope, counter);

template <class Id> struct Sample {
	static constexpr u64 value_mask = (1ull << 45) - 1;

	Sample(SampleType type, Id id, u64 value) {
		static_assert(count<SampleType> <= 8);
		encoded = (value & value_mask) | (u64(type) << 45) | (u64(id.value) << 48);
	}

	void setValue(u64 value) { encoded = (encoded & ~value_mask) | (value & value_mask); }

	u64 value() const { return encoded & ((1ull << 45) - 1); }
	SampleType type() const { return SampleType((encoded >> 45) & 0x7); }
	Id id() const { return Id(u16(encoded >> 48)); }

	void operator>>(TextFormatter &) const;

	u64 encoded;
};

using PSample = Sample<PointId>;
using ESample = Sample<ExecId>;

class ExecTree;
class ThreadContext;
class Manager;

struct Frame {
	vector<ESample> samples;
	double start_time = -1.0, end_time = -1.0;
	int frame_id, thread_id = 0;

	auto usedMemory() const { return samples.usedMemory(); }
};

struct FuncName {
	FuncName(Str);
	FuncName() {}

	Str return_type, args, name;
};

struct PointInfo {
	FuncName func;
	Str file, tag;
	int line;
	PointType type;
};

const PointInfo *pointInfo(PointId);
PointId registerPoint(PointType, const char *file, const char *func, const char *tag, int line);
template <class RegFunctor> struct RegisterPoint { static inline PointId id = RegFunctor::call(); };

// ------------------------------------------------------------------------------------------------
// Per thread control:
// ------------------------------------------------------------------------------------------------

void enterScope(PointId);
void exitScope(PointId);
void exitSingleScope(PointId);
void siblingScope(PointId);
void childScope(PointId);

void enterGpuScope(PointId);
void exitGpuScope(PointId);
void exitSingleGpuScope(PointId);
void siblingGpuScope(PointId);
void childGpuScope(PointId);

void setCounter(PointId, u64 value);

void endFrame();
void nextFrame();

// Make sure not to break scopes when pausing & resuming
void pause();
void resume();

void pauseGpu();
void resumeGpu();

// TODO: lepsze nazewnictwo ?
// TODO: co jeśli przy inicjalizacji statika odpalamy funkcję która jest instrumentowana?
//       punkty instrumentacji mogą jeszcze nie istnieć...
//       Nie da się też sprawdzić, czy perf jest włączony, bo nie wiadomo, czy wogóle jest zainicjowany

#ifdef FWK_PERF_DISABLE_SAMPLING
#define PERF_SCOPE_POINT(name, func, tag)

#define PERF_SCOPE(...)
#define PERF_GPU_SCOPE(...)

#define PERF_CHILD_SCOPE(...)
#define PERF_SIBLING_SCOPE(...)
#define PERF_POP_CHILD_SCOPE()
#define PERF_CLOSE_SCOPE()
#define PERF_COUNT(value, ...)
#define PERF_IF(...)
#else

// clang-format off
#define _PERF_ID(prefix, line) prefix##line 
#define _PERF_FUNC_ID(line, func)			constexpr const char * _PERF_ID(func_, line) = __PRETTY_FUNCTION__
#define _PERF_POINT_ID(ptype, line, tag)	struct _PERF_ID(point_id_, line) { static auto call() { return perf::registerPoint(perf::PointType::ptype, __FILE__, _PERF_ID(func_, line), tag, line); } };
#define _PERF_POINT(ptype, line, func, tag)	_PERF_FUNC_ID(line, func); _PERF_POINT_ID(ptype, line, tag); 
#define _PERF_USE_POINT(line)				perf::RegisterPoint<_PERF_ID(point_id_, line)>::id

#define PERF_SCOPE_POINT(name, func, tag) 	static const auto name = perf::registerPoint(perf::PointType::scope, __FILE__, func, tag, __LINE__);

// All of those PERF_* macros accept optional tag argument (it must be a C string)

// Defines a scope either with GPU measurements or without
#define PERF_SCOPE(...)						_PERF_POINT(scope, __LINE__, __PRETTY_FUNCTION__, "#" __VA_ARGS__);				perf::Scope perf_scope{_PERF_USE_POINT(__LINE__)};
#define PERF_GPU_SCOPE(...)					_PERF_POINT(scope, __LINE__, __PRETTY_FUNCTION__, "#" __VA_ARGS__);				perf::GpuScope perf_scope{_PERF_USE_POINT(__LINE__)};

// Overrides current scope
#define PERF_CHILD_SCOPE(...)				_PERF_POINT(scope, __LINE__, __PRETTY_FUNCTION__, "#" __VA_ARGS__);				perf_scope.child(_PERF_USE_POINT(__LINE__));
#define PERF_SIBLING_SCOPE(...)				_PERF_POINT(scope, __LINE__, __PRETTY_FUNCTION__, "#" __VA_ARGS__);				perf_scope.sibling(_PERF_USE_POINT(__LINE__));
#define PERF_POP_CHILD_SCOPE()				perf_scope.exitSingle();
#define PERF_CLOSE_SCOPE()					perf_scope.close();

#define PERF_COUNT(value, ...)				_PERF_POINT(counter, __LINE__, __PRETTY_FUNCTION__, #value "#" __VA_ARGS__);	perf::setCounter(_PERF_USE_POINT(__LINE__), value);
#define PERF_IF(...)						__VA_ARGS__

// clang-format on
#endif

struct ScopeInfo {};

class Scope {
  public:
	Scope(PointId id) : point_id(id) {
		PASSERT(point_id);
		enterScope(id);
	}
	~Scope() {
		if(point_id)
			exitScope(point_id);
	}

	void sibling(PointId id) {
		PASSERT(id);
		point_id = id;
		siblingScope(id);
	}
	void child(PointId id) {
		PASSERT(id);
		point_id = id;
		childScope(id);
	}
	void exitSingle() { exitSingleScope(point_id); }
	void close() {
		exitScope(point_id);
		point_id = {};
	}

  private:
	PointId point_id;
};

class GpuScope {
  public:
	GpuScope(PointId id) : point_id(id) {
		PASSERT(point_id);
		enterGpuScope(id);
	}
	~GpuScope() {
		if(point_id)
			exitGpuScope(point_id);
	}

	void sibling(PointId id) {
		PASSERT(id);
		point_id = id;
		siblingGpuScope(id);
	}
	void child(PointId id) {
		PASSERT(id);
		point_id = id;
		childGpuScope(id);
	}
	void exitSingle() { exitSingleGpuScope(point_id); }
	void close() {
		exitScope(point_id);
		point_id = {};
	}

  private:
	PointId point_id;
};
}

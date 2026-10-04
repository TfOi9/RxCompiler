#pragma once
#include "body_checker.hpp"
#include <optional>
#include <cassert>

namespace semantic {

class ReachabilityGuard {
public:
    explicit ReachabilityGuard(FunctionCheckContext& ctx, bool can_enter = true) noexcept:
            ctx_(ctx), saved_(ctx.path_reachable) {
        ctx_.path_reachable = saved_ && can_enter;
    }
    ~ReachabilityGuard() noexcept {
        ctx_.path_reachable = saved_;
    }

    ReachabilityGuard(const ReachabilityGuard&) = delete;
    ReachabilityGuard& operator=(const ReachabilityGuard&) = delete;
    ReachabilityGuard(ReachabilityGuard&&) = delete;
    ReachabilityGuard& operator=(ReachabilityGuard&&) = delete;

private:
    FunctionCheckContext& ctx_;
    bool saved_;
};

class LoopGuard {
public:
    LoopGuard(FunctionCheckContext& ctx, LoopKind kind, std::optional<TyId> expected_result):
            ctx_(ctx), index_(ctx.loops.size()), id_(ctx_.next_loop_id++) {
        ctx_.loops.push_back(LoopFrame{
            id_,
            kind,
            expected_result,
            {}
        });
    }
    ~LoopGuard() noexcept {
        assert(ctx_.loops.size() == index_ + 1);
        ctx_.loops.pop_back();
    }
    LoopId id() const noexcept {
        return id_;
    }
    LoopFrame takeFrame() {
        assert(ctx_.loops.size() == index_ + 1);
        return std::move(ctx_.loops[index_]);
    }

    LoopGuard(const LoopGuard&) = delete;
    LoopGuard& operator=(const LoopGuard&) = delete;
    LoopGuard(LoopGuard&&) = delete;
    LoopGuard& operator=(LoopGuard&&) = delete;

private:
    FunctionCheckContext& ctx_;
    size_t index_;
    LoopId id_;
};

class LoopTargetFloorGuard {
public:
    LoopTargetFloorGuard(FunctionCheckContext &ctx, size_t floor) noexcept:
            ctx_(ctx), saved_(ctx.loop_target_floor) {
        assert(floor <= ctx_.loops.size());
        assert(floor >= saved_);
        ctx_.loop_target_floor = floor;
    }
    ~LoopTargetFloorGuard() {
        ctx_.loop_target_floor = saved_;
    }

    LoopTargetFloorGuard(const LoopTargetFloorGuard&) = delete;
    LoopTargetFloorGuard& operator=(const LoopTargetFloorGuard&) = delete;
    LoopTargetFloorGuard(LoopTargetFloorGuard&&) = delete;
    LoopTargetFloorGuard& operator=(LoopTargetFloorGuard&&) = delete;
private:
    FunctionCheckContext& ctx_;
    size_t saved_;
};

} // namespace semantic
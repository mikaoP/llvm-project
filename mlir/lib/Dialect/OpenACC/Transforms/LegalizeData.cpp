//===- LegalizeData.cpp - -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/OpenACC/Transforms/Passes.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/OpenACC/OpenACC.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/RegionUtils.h"

namespace mlir {
namespace acc {
#define GEN_PASS_DEF_LEGALIZEDATAINREGION
#include "mlir/Dialect/OpenACC/Transforms/Passes.h.inc"
} // namespace acc
} // namespace mlir

using namespace mlir;

namespace {

template <typename Op>
static void collectAndReplaceInRegion(Op &op, bool hostToDevice) {
  llvm::SmallVector<std::pair<Value, Value>> values;
  for (auto operand : op.getDataClauseOperands()) {
    Value varPtr = acc::getVarPtr(operand.getDefiningOp());
    Value accPtr = acc::getAccPtr(operand.getDefiningOp());
    if (varPtr && accPtr) {
      if (hostToDevice)
        values.push_back({varPtr, accPtr});
      else
        values.push_back({accPtr, varPtr});
    }
  }

  for (auto p : values)
    replaceAllUsesInRegionWith(std::get<0>(p), std::get<1>(p), op.getRegion());
}

struct LegalizeDataInRegion
    : public acc::impl::LegalizeDataInRegionBase<LegalizeDataInRegion> {

  void runOnOperation() override {
    func::FuncOp funcOp = getOperation();
    bool replaceHostVsDevice = this->hostToDevice.getValue();

    funcOp.walk([&](Operation *op) {
      if (!isa<ACC_COMPUTE_CONSTRUCT_OPS>(*op))
        return;

      if (auto parallelOp = dyn_cast<acc::ParallelOp>(*op)) {
        collectAndReplaceInRegion(parallelOp, replaceHostVsDevice);
      } else if (auto serialOp = dyn_cast<acc::SerialOp>(*op)) {
        collectAndReplaceInRegion(serialOp, replaceHostVsDevice);
      } else if (auto kernelsOp = dyn_cast<acc::KernelsOp>(*op)) {
        collectAndReplaceInRegion(kernelsOp, replaceHostVsDevice);
      }
    });
  }
};

} // end anonymous namespace

std::unique_ptr<OperationPass<func::FuncOp>>
mlir::acc::createLegalizeDataInRegion() {
  return std::make_unique<LegalizeDataInRegion>();
}

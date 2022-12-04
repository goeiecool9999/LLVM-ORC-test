#include <iostream>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <unistd.h>

int main() {
  LLVM_NATIVE_TARGET();
  LLVM_NATIVE_ASMPRINTER();
  LLVM_NATIVE_ASMPARSER();
  LLVM_NATIVE_TARGETMC();
  LLVM_NATIVE_TARGET();
  LLVM_NATIVE_TARGETINFO();
  LLVM_NATIVE_DISASSEMBLER();

  using namespace llvm::orc;
  using namespace llvm;
  auto EPC = SelfExecutorProcessControl::Create();
  auto ES = std::make_unique<ExecutionSession>(std::move(*EPC));
  RTDyldObjectLinkingLayer ObjLinkingLayer(
    *ES, []() { return std::make_unique<SectionMemoryManager>(); });

  llvm::raw_fd_ostream errstream{STDERR_FILENO, false};

  auto tmb = JITTargetMachineBuilder::detectHost();
  logAllUnhandledErrors(tmb.takeError(), errstream);
  if(!tmb)
    return 1;
  std::cout << tmb->getCPU() << std::endl;
  std::cout << "triple: " << tmb->getTargetTriple().normalize() << std::endl;
  auto targetmachine = tmb->createTargetMachine();
  logAllUnhandledErrors(targetmachine.takeError(), errstream);
  if(!targetmachine)
    return 1;

  auto layout = targetmachine.get()->createDataLayout();

  auto compiler = std::make_unique<SimpleCompiler>(*targetmachine.get());
  IRCompileLayer IRLayer(*ES, ObjLinkingLayer, std::move(compiler));

  auto context = std::make_unique<LLVMContext>();

// Create JITDylib "A" and add code to it using the IR layer.
  auto& MainJD = ES->createJITDylib("A").get();
  auto procGen = DynamicLibrarySearchGenerator::GetForCurrentProcess(layout.getGlobalPrefix());
  logAllUnhandledErrors(procGen.takeError(), errstream);
  if(!procGen)
    return 1;
  MainJD.addGenerator(std::move(procGen.get()));

  llvm::SMDiagnostic err;
  auto module = llvm::parseIRFile("asdf.llvm", err, *context);

  if(!module)
  {
    err.print("asdf.llvm", errstream);
    return 1;
  }

  llvm::orc::ThreadSafeModule tsm{std::move(module), std::move(context)};

  logAllUnhandledErrors(IRLayer.add(MainJD, std::move(tsm)), errstream);

// Look up the JIT'd main, cast it to a function pointer, then call it.
  auto MainSym = ES->lookup({&MainJD}, "_start");

  auto* Main = (void (*)()) MainSym->getAddress();

  Main();
}

int main() {
  LLVM_NATIVE_TARGET();
  LLVM_NATIVE_ASMPRINTER();
  LLVM_NATIVE_TARGETMC();
  LLVM_NATIVE_TARGETINFO();

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
  auto module = llvm::parseIRFile("code.ll", err, *context);

  if(!module)
  {
    err.print("code.ll", errstream);
    return 1;
  }

  llvm::orc::ThreadSafeModule tsm{std::move(module), std::move(context)};

  logAllUnhandledErrors(IRLayer.add(MainJD, std::move(tsm)), errstream);

// Look up the JIT'd main, cast it to a function pointer, then call it.
  auto MainSym = ES->lookup({&MainJD}, "_start");

  auto* Main = (void (*)()) MainSym->getAddress();

  Main();

  auto context_1 = std::make_unique<llvm::LLVMContext>();
  auto testmod = std::make_unique<llvm::Module>("irfromcode", *context_1);
  auto test = Function::Create(FunctionType::get(llvm::Type::getVoidTy(*context_1), false), llvm::GlobalValue::CommonLinkage, "testfunc", *testmod);

  auto putchar_callee = testmod->getOrInsertFunction("putchar", FunctionType::get(Type::getInt32Ty(*context_1), {Type::getInt32Ty(*context_1)}, false));

  auto helloWorldConst = ConstantDataArray::getString(*context_1, "Hello world\n");
  auto dataglobal = new GlobalVariable {*testmod, helloWorldConst->getType(), true, llvm::GlobalVariable::InternalLinkage, helloWorldConst, "data"};

  auto entry = BasicBlock::Create(*context_1, "", test);
  auto Loop = BasicBlock::Create(*context_1, "Loop", test);
  auto LoopBody = BasicBlock::Create(*context_1, "LoopBody", test);
  auto Exit = BasicBlock::Create(*context_1, "Exit", test);

  auto builder = std::make_unique<IRBuilder<>>(entry);
  // allocate iterator pointer
  auto ptr_alloca = builder->CreateAlloca(builder->getInt8PtrTy(), nullptr, "ptr");
  // load initial value into pointer
  builder->CreateStore(dataglobal, ptr_alloca);
  builder->CreateBr(Loop);

  builder.reset(new IRBuilder<>{Loop});
  // load current value of iterator pointer
  auto current_ptr = builder->CreateLoad(builder->getInt8PtrTy(), ptr_alloca, "currentptr");
  // load the current char pointer to by pointer
  auto current_ptr_val = builder->CreateLoad(builder->getInt8Ty(), current_ptr, "currentptrval");
  // convert the current char val to 32-bit int
  auto char_to_int = builder->CreateBitCast(current_ptr_val, builder->getInt32Ty(), "currentptrvalint");


  auto compare_null = builder->CreateCmp(CmpInst::Predicate::ICMP_EQ, char_to_int, ConstantInt::get(builder->getInt32Ty(), 0), "compare_res");
  auto incremented_ptr = builder->CreateAdd(current_ptr, ConstantInt::get(builder->getInt32Ty(), 1));
  auto store_added_ptr = builder->CreateStore(incremented_ptr, ptr_alloca);

  builder->CreateCondBr(compare_null, Exit, LoopBody);

  builder.reset(new IRBuilder<>{LoopBody});
  builder->CreateCall(putchar_callee, {char_to_int});
  builder->CreateBr(Loop);

  builder.reset(new IRBuilder<>{Exit});
  builder->CreateRetVoid();

  testmod->print(errstream, nullptr);
  ThreadSafeModule tsm_2{std::move(testmod), std::move(context_1)};

  auto genmoderr = IRLayer.add(MainJD, std::move(tsm_2));
  logAllUnhandledErrors(std::move(genmoderr), errstream);
  if(genmoderr)
    return 1;

  auto TestSym = ES->lookup({&MainJD}, "testfunc");
  if(!TestSym)
    return 1;
  auto* Test = (void(*)()) TestSym->getAddress();
  Test();

}

; ModuleID = 'test.c'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@data = unnamed_addr constant [15 x i8] c"Hello, World!\0A\00", align 1

define void @_start() {
  %ptr = alloca i8*
  %firstelement = bitcast [15 x i8]* @data to i8*;
  store i8* %firstelement, i8** %ptr
  br label %Repeat

Repeat:
  ;current value of the pointer on stack
  %currentptr = load i8*, i8** %ptr
  ;value of the char pointed to
  %currentchar = load i8, i8* %currentptr
  ;converted to int for putchar
  %currentcharint = sext i8 %currentchar to i32

  ;is character null terminator?
  %is_terminator = icmp eq i8 %currentchar, 0
  br i1 %is_terminator, label %Exit, label %LoopBody

LoopBody:
  ;output character
  call i32 @putchar(i32 %currentcharint)

  ;increment char ptr
  %newptr = getelementptr i8, i8* %currentptr, i8 1
  store i8* %newptr, i8** %ptr
  br label %Repeat

Exit:
  ret void
}

declare i32 @putchar(i32)
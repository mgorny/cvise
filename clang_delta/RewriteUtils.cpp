//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RewriteUtils.h"

#include <cctype>
#include <sstream>
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Rewriter.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/TypeLoc.h"

using namespace clang;
using namespace llvm;

static const char *DefaultIndentStr = "    ";

RewriteUtils *RewriteUtils::Instance;

const char *RewriteUtils::TmpVarNamePrefix = "__trans_tmp_";

RewriteUtils *RewriteUtils::GetInstance(Rewriter *RW)
{
  if (RewriteUtils::Instance) {
    RewriteUtils::Instance->TheRewriter = RW;
    RewriteUtils::Instance->SrcManager = &(RW->getSourceMgr());
    return RewriteUtils::Instance;
  }

  RewriteUtils::Instance = new RewriteUtils();
  assert(RewriteUtils::Instance);

  RewriteUtils::Instance->TheRewriter = RW;
  RewriteUtils::Instance->SrcManager = &(RW->getSourceMgr());
  return RewriteUtils::Instance;
}

void RewriteUtils::Finalize(void)
{
  if (RewriteUtils::Instance) {
    delete RewriteUtils::Instance;
    RewriteUtils::Instance = NULL;
  }
}

// copied from Rewriter.cpp
unsigned RewriteUtils::getLocationOffsetAndFileID(SourceLocation Loc,
                                                  FileID &FID,
                                                  SourceManager *SrcManager)
{
  assert(Loc.isValid() && "Invalid location");
  std::pair<FileID,unsigned> V = SrcManager->getDecomposedLoc(Loc);
  FID = V.first;
  return V.second;
}

unsigned RewriteUtils::getOffsetBetweenLocations(SourceLocation StartLoc,
                                            SourceLocation EndLoc,
                                            SourceManager *SrcManager)
{
  FileID FID;
  unsigned StartOffset = 
    getLocationOffsetAndFileID(StartLoc, FID, SrcManager);
  unsigned EndOffset = 
    getLocationOffsetAndFileID(EndLoc, FID, SrcManager);
  TransAssert((EndOffset >= StartOffset) && "Bad locations!");
  return (EndOffset - StartOffset);
}

SourceLocation RewriteUtils::getEndLocationFromBegin(SourceRange Range)
{
  int LocRangeSize = TheRewriter->getRangeSize(Range);
  if (LocRangeSize == -1)
    return Range.getEnd();

  SourceLocation StartLoc = Range.getBegin();

  return StartLoc.getLocWithOffset(LocRangeSize);
}

int RewriteUtils::getOffsetUntil(const char *Buf, char Symbol)
{
  int Offset = 0;
  while (*Buf != Symbol) {
    Buf++;
    Offset++;
  }
  return Offset;
}

int RewriteUtils::getSkippingOffset(const char *Buf, char Symbol)
{
  int Offset = 0;
  while (*Buf == Symbol) {
    Buf++;
    Offset++;
  }
  return Offset;
}

SourceLocation RewriteUtils::getEndLocationUntil(SourceRange Range, 
                                                 char Symbol)
{
  SourceLocation EndLoc = getEndLocationFromBegin(Range);
    
  const char *EndBuf = SrcManager->getCharacterData(EndLoc);
  int Offset = getOffsetUntil(EndBuf, Symbol);
  return EndLoc.getLocWithOffset(Offset);
}

SourceLocation RewriteUtils::getLocationUntil(SourceLocation Loc, 
                                              char Symbol)
{
  const char *Buf = SrcManager->getCharacterData(Loc);
  int Offset = getOffsetUntil(Buf, Symbol);
  return Loc.getLocWithOffset(Offset);
}

SourceLocation RewriteUtils::getEndLocationAfter(SourceRange Range, 
                                                char Symbol)
{
  SourceLocation EndLoc = getEndLocationFromBegin(Range);
    
  const char *EndBuf = SrcManager->getCharacterData(EndLoc);
  int Offset = getOffsetUntil(EndBuf, Symbol);
  Offset++;
  return EndLoc.getLocWithOffset(Offset);
}

SourceLocation RewriteUtils::getLocationAfter(SourceLocation StartLoc, 
                                              char Symbol)
{
  const char *StartBuf = SrcManager->getCharacterData(StartLoc);
  int Offset = getSkippingOffset(StartBuf, Symbol);
  return StartLoc.getLocWithOffset(Offset);
}

SourceLocation RewriteUtils::getParamSubstringLocation(SourceLocation StartLoc,
                                              size_t Size,
                                              const std::string &Substr)
{
  const char *StartBuf = SrcManager->getCharacterData(StartLoc);
  std::string TmpStr(StartBuf, Size);

  size_t Pos = TmpStr.find(Substr);
  TransAssert((Pos != std::string::npos) && "Bad Name Position!");

  if (Pos == 0)
    return StartLoc;
  else 
    return StartLoc.getLocWithOffset(Pos);
}

bool RewriteUtils::removeParamFromFuncDecl(const ParmVarDecl *PV,
                                           unsigned int NumParams,
                                           int ParamPos)
{
  SourceRange ParamLocRange = PV->getSourceRange();
  int RangeSize;
 
  SourceLocation StartLoc = ParamLocRange.getBegin();
  if (StartLoc.isInvalid()) {
    StartLoc = ParamLocRange.getEnd();
    RangeSize = PV->getNameAsString().size();
  }
  else {
    RangeSize = TheRewriter->getRangeSize(ParamLocRange);
    if (RangeSize == -1)
      return false;
  }

  // The param is the only parameter of the function declaration.
  // Replace it with void
  if ((ParamPos == 0) && (NumParams == 1)) {
    // Note that ')' is included in ParamLocRange for unnamed parameter
    if (PV->getDeclName())
      return !(TheRewriter->ReplaceText(StartLoc,
                                        RangeSize, "void"));
    else
      return !(TheRewriter->ReplaceText(StartLoc,
                                        RangeSize - 1, "void"));
  }

  // The param is the last parameter
  if (ParamPos == static_cast<int>(NumParams - 1)) {
    int Offset = 0;
    const char *StartBuf = 
      SrcManager->getCharacterData(StartLoc);

    TransAssert(StartBuf && "Invalid start buffer!");
    while (*StartBuf != ',') {
      StartBuf--;
      Offset--;
    }

    SourceLocation NewStartLoc = StartLoc.getLocWithOffset(Offset);

    // Note that ')' is included in ParamLocRange for unnamed parameter
    if (PV->getDeclName())
      return !(TheRewriter->RemoveText(NewStartLoc, 
                                       RangeSize - Offset));
    else
      return !(TheRewriter->RemoveText(NewStartLoc, 
                                       RangeSize - Offset - 1));
  }
 
  // Clang gives inconsistent RangeSize for named and unnamed parameter decls.
  // For example, for the first parameter, 
  //   foo(int, int);  -- RangeSize is 4, i.e., "," is counted
  //   foo(int x, int);  -- RangeSize is 5, i.e., ","is not included
  if (PV->getDeclName()) {
    // We cannot use the code below:
    //   SourceLocation EndLoc = ParamLocRange.getEnd();
    //   const char *EndBuf = 
    //     ConsumerInstance->SrcManager->getCharacterData(EndLoc);
    // Because getEnd() returns the start of the last token if this
    // is a token range. For example, in the above example, 
    // getEnd() points to the start of "x"
    // See the comments on getRangeSize in clang/lib/Rewriter/Rewriter.cpp
    int NewRangeSize = 0;
    const char *StartBuf = 
      SrcManager->getCharacterData(StartLoc);

    while (NewRangeSize < RangeSize) {
      StartBuf++;
      NewRangeSize++;
    }

    TransAssert(StartBuf && "Invalid start buffer!");
    while (*StartBuf != ',') {
      StartBuf++;
      NewRangeSize++;
    }

    return !(TheRewriter->RemoveText(StartLoc, 
                                     NewRangeSize + 1));
  }
  else {
    return !(TheRewriter->RemoveText(StartLoc, RangeSize));
  }
}

bool RewriteUtils::removeArgFromCallExpr(CallExpr *CallE,
                                        int ParamPos)
{
  if (ParamPos >= static_cast<int>(CallE->getNumArgs()))
    return true;

  Expr *Arg = CallE->getArg(ParamPos);
  TransAssert(Arg && "Null arg!");

  SourceRange ArgRange = Arg->getSourceRange();
  int RangeSize = TheRewriter->getRangeSize(ArgRange);

  if (RangeSize == -1)
    return false;

  SourceLocation StartLoc = ArgRange.getBegin();
  unsigned int NumArgs = CallE->getNumArgs();

  if ((ParamPos == 0) && (NumArgs == 1)) {
    // Note that ')' is included in ParamLocRange
    return !(TheRewriter->RemoveText(ArgRange));
  }

  // The param is the last parameter
  if (ParamPos == static_cast<int>(NumArgs - 1)) {
    int Offset = 0;
    const char *StartBuf = SrcManager->getCharacterData(StartLoc);

    TransAssert(StartBuf && "Invalid start buffer!");
    while (*StartBuf != ',') {
      StartBuf--;
      Offset--;
    }

    SourceLocation NewStartLoc = StartLoc.getLocWithOffset(Offset);
    return !(TheRewriter->RemoveText(NewStartLoc,
                                     RangeSize - Offset));
  }

  // We cannot use SrcManager->getCharacterData(StartLoc) to get the buffer,
  // because it returns the unmodified string. I've tried to use 
  // getEndlocationUntil(ArgRange, ",", ...) call, but still failed. 
  // Seems in some cases, it returns bad results for a complex case like:
  //  foo(...foo(...), ...)
  // So I ended up with this ugly way - get the end loc from the next arg.
  Expr *NextArg = CallE->getArg(ParamPos+1);
  SourceRange NextArgRange = NextArg->getSourceRange();
  SourceLocation NextStartLoc = NextArgRange.getBegin();
  const char *NextStartBuf = SrcManager->getCharacterData(NextStartLoc);
  int Offset = 0;
  while (*NextStartBuf != ',') {
      NextStartBuf--;
      Offset--;
  }

  SourceLocation NewEndLoc = NextStartLoc.getLocWithOffset(Offset);
  return !TheRewriter->RemoveText(SourceRange(StartLoc, NewEndLoc));
}

SourceLocation RewriteUtils::getVarDeclTypeLocEnd(const VarDecl *VD)
{
  TypeLoc VarTypeLoc = VD->getTypeSourceInfo()->getTypeLoc();

  TypeLoc NextTL = VarTypeLoc.getNextTypeLoc();
  while (!NextTL.isNull()) {
    VarTypeLoc = NextTL;
    NextTL = NextTL.getNextTypeLoc();
  }

  SourceRange TypeLocRange = VarTypeLoc.getSourceRange();
  SourceLocation EndLoc = 
    getEndLocationFromBegin(TypeLocRange);
  return EndLoc;
}

bool RewriteUtils::removeVarFromDeclStmt(DeclStmt *DS,
                                         const VarDecl *VD,
                                         Decl *PrevDecl,
                                         bool IsFirstDecl)
{
  SourceRange StmtRange = DS->getSourceRange();

  // VD is the the only declaration, so it is safe to remove the entire stmt
  if (DS->isSingleDecl()) {
    return !(TheRewriter->RemoveText(StmtRange));
  }

  SourceRange VarRange = VD->getSourceRange();

  // VD is the first declaration in a declaration group.
  // We keep the leading type string
  if (IsFirstDecl) {
    // We need to get the outermost TypeLocEnd instead of the StartLoc of
    // a var name, because we need to handle the case below:
    //   int *x, *y;
    // If we rely on the StartLoc of a var name, then we will make bad
    // transformation like:
    //   int * *y;
    SourceLocation NewStartLoc = getVarDeclTypeLocEnd(VD);

    SourceLocation NewEndLoc = getEndLocationUntil(VarRange, ',');
    
    return 
      !(TheRewriter->RemoveText(SourceRange(NewStartLoc, NewEndLoc)));
  }

  TransAssert(PrevDecl && "PrevDecl cannot be NULL!");
  SourceLocation VarEndLoc = VarRange.getEnd();
  SourceRange PrevDeclRange = PrevDecl->getSourceRange();

  SourceLocation PrevDeclEndLoc = getEndLocationUntil(PrevDeclRange, ',');

  return !(TheRewriter->RemoveText(SourceRange(PrevDeclEndLoc, VarEndLoc)));
}

bool RewriteUtils::getExprString(const Expr *E, 
                                 std::string &ES)
{
  SourceRange ExprRange = E->getSourceRange();
   
  int RangeSize = TheRewriter->getRangeSize(ExprRange);
  if (RangeSize == -1)
    return false;

  SourceLocation StartLoc = ExprRange.getBegin();
  const char *StartBuf = SrcManager->getCharacterData(StartLoc);

  ES.assign(StartBuf, RangeSize);

  const BinaryOperator *BinOp = dyn_cast<BinaryOperator>(E);

  // Keep the semantics of Comma operator
  if (BinOp && (BinOp->getOpcode() == BO_Comma))
    ES = "(" + ES + ")";

  return true;
}

bool RewriteUtils::getStmtString(const Stmt *S, 
                                 std::string &Str)
{
  SourceRange StmtRange = S->getSourceRange();
   
  int RangeSize = TheRewriter->getRangeSize(StmtRange);
  if (RangeSize == -1)
    return false;

  SourceLocation StartLoc = StmtRange.getBegin();
  const char *StartBuf = SrcManager->getCharacterData(StartLoc);

  Str.assign(StartBuf, RangeSize);

  return true;
}

bool RewriteUtils::replaceExpr(const Expr *E, 
                               const std::string &ES)
{
  SourceRange ExprRange = E->getSourceRange();
   
  int RangeSize = TheRewriter->getRangeSize(ExprRange);
  if (RangeSize == -1)
    return false;

  return !(TheRewriter->ReplaceText(ExprRange, ES));
}

bool RewriteUtils::replaceExprNotInclude(const Expr *E, 
                               const std::string &ES)
{
  SourceRange ExprRange = E->getSourceRange();
  SourceLocation StartLoc = ExprRange.getBegin();
  int RangeSize = TheRewriter->getRangeSize(ExprRange);
  TransAssert((RangeSize != -1) && "Bad expr range!");

  Rewriter::RewriteOptions Opts;
  // We don't want to include the previously inserted string
  Opts.IncludeInsertsAtBeginOfRange = false;

  TheRewriter->RemoveText(ExprRange, Opts);
  return !(TheRewriter->InsertText(StartLoc, ES));
}

std::string RewriteUtils::getStmtIndentString(Stmt *S,
                                          SourceManager *SrcManager)
{
  SourceLocation StmtStartLoc = S->getLocStart();

  FileID FID;
  unsigned StartOffset = 
    getLocationOffsetAndFileID(StmtStartLoc, FID, SrcManager);

  StringRef MB = SrcManager->getBufferData(FID);
 
  unsigned lineNo = SrcManager->getLineNumber(FID, StartOffset) - 1;
  const SrcMgr::ContentCache *
      Content = SrcManager->getSLocEntry(FID).getFile().getContentCache();
  unsigned lineOffs = Content->SourceLineCache[lineNo];
 
  // Find the whitespace at the start of the line.
  StringRef indentSpace;

  unsigned I = lineOffs;
  while (isspace(MB[I]))
    ++I;
  indentSpace = MB.substr(lineOffs, I-lineOffs);

  return indentSpace;
}

bool RewriteUtils::addLocalVarToFunc(const std::string &VarStr,
                                     FunctionDecl *FD)
{
  Stmt *Body = FD->getBody();
  TransAssert(Body && "NULL body for a function definition!");

  std::string IndentStr;
  StmtIterator I = Body->child_begin();

  if (I == Body->child_end())
    IndentStr = DefaultIndentStr;
  else
    IndentStr = getStmtIndentString((*I), SrcManager);

  std::string NewVarStr = "\n" + IndentStr + VarStr;
  SourceLocation StartLoc = Body->getLocStart();
  return !(TheRewriter->InsertTextAfterToken(StartLoc, NewVarStr));
}

const char *RewriteUtils::getTmpVarNamePrefix(void)
{
  return TmpVarNamePrefix;
}

bool RewriteUtils::addNewAssignStmtBefore(Stmt *BeforeStmt,
                                          const std::string &VarName,
                                          Expr *RHS,
                                          bool NeedParen)
{
  std::string IndentStr = 
    RewriteUtils::getStmtIndentString(BeforeStmt, SrcManager);

  if (NeedParen) {
    SourceRange StmtRange = BeforeStmt->getSourceRange();
    SourceLocation LocEnd = 
      RewriteUtils::getEndLocationFromBegin(StmtRange);

    std::string PostStr = "\n" + IndentStr + "}";
    if (TheRewriter->InsertTextAfterToken(LocEnd, PostStr))
      return false;
  }

  SourceLocation StmtLocStart = BeforeStmt->getLocStart();

  std::string ExprStr;
  RewriteUtils::getExprString(RHS, ExprStr);

  std::string AssignStmtStr;
  
  if (NeedParen) {
    AssignStmtStr = "{\n";
    AssignStmtStr += IndentStr + "  " + VarName + " = ";
    AssignStmtStr += ExprStr;
    AssignStmtStr += ";\n" + IndentStr + "  ";
  }
  else {
    AssignStmtStr = VarName + " = ";
    AssignStmtStr += ExprStr;
    AssignStmtStr += ";\n" + IndentStr;
  }
  
  return !(TheRewriter->InsertText(StmtLocStart, 
             AssignStmtStr, /*InsertAfter=*/false));
}

void RewriteUtils::indentAfterNewLine(StringRef Str,
                                      std::string &NewStr,
                                      const std::string &IndentStr)
{
  SmallVector<StringRef, 20> StrVec;
  Str.split(StrVec, "\n"); 
  NewStr = "";
  for(SmallVector<StringRef, 20>::iterator I = StrVec.begin(), 
      E = StrVec.end(); I != E; ++I) {
    NewStr += ((*I).str() + "\n" + IndentStr);
  }
}

bool RewriteUtils::addStringBeforeStmt(Stmt *BeforeStmt,
                                   const std::string &Str,
                                   bool NeedParen)
{
  std::string IndentStr = 
    RewriteUtils::getStmtIndentString(BeforeStmt, SrcManager);

  if (NeedParen) {
    SourceRange StmtRange = BeforeStmt->getSourceRange();
    SourceLocation LocEnd = 
      RewriteUtils::getEndLocationFromBegin(StmtRange);

    std::string PostStr = "\n" + IndentStr + "}";
    if (TheRewriter->InsertTextAfterToken(LocEnd, PostStr))
      return false;
  }

  SourceLocation StmtLocStart = BeforeStmt->getLocStart();

  std::string NewStr;

  if (NeedParen) {
    NewStr = "{\n";
  }
  NewStr += Str;
  NewStr += "\n";
  
  std::string IndentedStr;
  indentAfterNewLine(NewStr, IndentedStr, IndentStr);
  return !(TheRewriter->InsertText(StmtLocStart, 
             IndentedStr, /*InsertAfter=*/false));
}

bool RewriteUtils::addStringAfterStmt(Stmt *AfterStmt, 
                                      const std::string &Str)
{
  std::string IndentStr = 
    RewriteUtils::getStmtIndentString(AfterStmt, SrcManager);

  std::string NewStr = "\n" + IndentStr + Str;
  SourceRange StmtRange = AfterStmt->getSourceRange();
  SourceLocation LocEnd = 
    RewriteUtils::getEndLocationFromBegin(StmtRange);
  
  return !(TheRewriter->InsertText(LocEnd, NewStr));
}

bool RewriteUtils::addStringAfterVarDecl(VarDecl *VD,
                                         const std::string &Str)
{
  SourceRange VarRange = VD->getSourceRange();
  SourceLocation LocEnd = RewriteUtils::getEndLocationAfter(VarRange, ';');
  
  return !(TheRewriter->InsertText(LocEnd, "\n" + Str));
}

bool RewriteUtils::replaceVarDeclName(VarDecl *VD,
                                      const std::string &NameStr)
{
  SourceLocation NameLocStart = VD->getLocation();
  return !(TheRewriter->ReplaceText(NameLocStart, 
             VD->getNameAsString().size(), NameStr));
}

bool RewriteUtils::replaceFunctionDeclName(FunctionDecl *FD,
                                      const std::string &NameStr)
{
  return !TheRewriter->ReplaceText(FD->getNameInfo().getLoc(),
                                   FD->getNameAsString().length(),
                                   NameStr);
}

void RewriteUtils::getStringBetweenLocs(std::string &Str, 
                                        SourceLocation LocStart,
                                        SourceLocation LocEnd)
{
  const char *StartBuf = SrcManager->getCharacterData(LocStart);
  const char *EndBuf = SrcManager->getCharacterData(LocEnd);
  TransAssert(StartBuf < EndBuf);
  size_t Off = EndBuf - StartBuf;
  Str.assign(StartBuf, Off);
}

void RewriteUtils::getStringBetweenLocsAfterStart(std::string &Str, 
                                                  SourceLocation LocStart,
                                                  SourceLocation LocEnd)
{
  const char *StartBuf = SrcManager->getCharacterData(LocStart);
  const char *EndBuf = SrcManager->getCharacterData(LocEnd);
  StartBuf++;
  TransAssert(StartBuf <= EndBuf);
  size_t Off = EndBuf - StartBuf;
  Str.assign(StartBuf, Off);
}

bool RewriteUtils::getDeclGroupStrAndRemove(DeclGroupRef DGR, 
                                   std::string &Str)
{
  if (DGR.isSingleDecl()) {
    Decl *D = DGR.getSingleDecl();
    VarDecl *VD = dyn_cast<VarDecl>(D);
    TransAssert(VD && "Bad VarDecl!");

    // We need to get the outermost TypeLocEnd instead of the StartLoc of
    // a var name, because we need to handle the case below:
    //   int *x;
    //   int *y;
    // If we rely on the StartLoc of a var name, then we will make bad
    // transformation like:
    //   int *x, y;
    SourceLocation TypeLocEnd = getVarDeclTypeLocEnd(VD);
    SourceRange VarRange = VD->getSourceRange();

    SourceLocation LocEnd = getEndLocationUntil(VarRange, ';');

    getStringBetweenLocs(Str, TypeLocEnd, LocEnd);

    SourceLocation StartLoc = VarRange.getBegin();
    SourceLocation NewEndLoc = getLocationAfter(LocEnd, ';');
    return !(TheRewriter->RemoveText(SourceRange(StartLoc, NewEndLoc)));
  }

  DeclGroup DG = DGR.getDeclGroup();
  size_t Sz = DG.size();
  TransAssert(Sz > 1);

  DeclGroupRef::iterator I = DGR.begin();
  DeclGroupRef::iterator E = DGR.end();
  --E;

  Decl *FirstD = (*I);
  VarDecl *FirstVD = dyn_cast<VarDecl>(FirstD);
  Decl *LastD = (*E);
  VarDecl *LastVD = dyn_cast<VarDecl>(LastD);

  TransAssert(FirstVD && "Bad First VarDecl!");
  TransAssert(LastVD && "Bad First VarDecl!");

  SourceLocation TypeLocEnd = getVarDeclTypeLocEnd(FirstVD);
  SourceRange LastVarRange = LastVD->getSourceRange();
  SourceLocation LastEndLoc = getEndLocationUntil(LastVarRange, ';');
  getStringBetweenLocs(Str, TypeLocEnd, LastEndLoc);

  SourceLocation StartLoc = FirstVD->getLocStart();
  SourceLocation NewLastEndLoc = getLocationAfter(LastEndLoc, ';');
  return !(TheRewriter->RemoveText(SourceRange(StartLoc, NewLastEndLoc)));
}

SourceLocation RewriteUtils::getDeclGroupRefEndLoc(DeclGroupRef DGR)
{
  Decl *LastD;

  if (DGR.isSingleDecl()) {
    LastD = DGR.getSingleDecl();
  }
  else {
    DeclGroupRef::iterator E = DGR.end();
    --E;
    LastD = (*E);
  }

  VarDecl *VD = dyn_cast<VarDecl>(LastD);
  TransAssert(VD && "Bad VD!");
  SourceRange VarRange = VD->getSourceRange();
  return getEndLocationFromBegin(VarRange);
}

SourceLocation RewriteUtils::getDeclStmtEndLoc(DeclStmt *DS)
{
  DeclGroupRef DGR = DS->getDeclGroup();
  return getDeclGroupRefEndLoc(DGR);
}

bool RewriteUtils::getDeclStmtStrAndRemove(DeclStmt *DS, 
                                   std::string &Str)
{
  DeclGroupRef DGR = DS->getDeclGroup();
  return getDeclGroupStrAndRemove(DGR, Str);
}

bool RewriteUtils::removeAStarBefore(const Decl *D)
{
  SourceLocation LocStart = D->getLocation();
  const char *StartBuf = SrcManager->getCharacterData(LocStart);
  int Offset = 0;
  while (*StartBuf != '*') {
    StartBuf--;
    Offset--;
  }
  SourceLocation StarLoc =  LocStart.getLocWithOffset(Offset);
  return !TheRewriter->RemoveText(StarLoc, 1);
}

bool RewriteUtils::removeASymbolAfter(const Expr *E,
                                    char Symbol)
{
  SourceRange ExprRange = E->getSourceRange();
  SourceLocation LocStart = ExprRange.getBegin();
  const char *StartBuf = SrcManager->getCharacterData(LocStart);
  int Offset = 0;
  while (*StartBuf != Symbol) {
    StartBuf++;
    Offset++;
  }
  SourceLocation StarLoc =  LocStart.getLocWithOffset(Offset);
  return !TheRewriter->RemoveText(StarLoc, 1);
}

bool RewriteUtils::removeAStarAfter(const Expr *E)
{
  return removeASymbolAfter(E, '*');
}

bool RewriteUtils::removeAnAddrOfAfter(const Expr *E)
{
  return removeASymbolAfter(E, '&');
}

bool RewriteUtils::insertAnAddrOfBefore(const DeclRefExpr *DRE)
{
  SourceRange ExprRange = DRE->getSourceRange();
  SourceLocation LocStart = ExprRange.getBegin();
  return !TheRewriter->InsertTextBefore(LocStart, "&");
}

bool RewriteUtils::insertAStarBefore(const Expr *E)
{
  SourceRange ExprRange = E->getSourceRange();
  SourceLocation LocStart = ExprRange.getBegin();
  return !TheRewriter->InsertTextBefore(LocStart, "*");
}

bool RewriteUtils::removeVarInitExpr(const VarDecl *VD)
{
  TransAssert(VD->hasInit() && "VarDecl doesn't have an Init Expr!");
  SourceLocation NameStartLoc = VD->getLocation();

  SourceLocation InitStartLoc = getLocationUntil(NameStartLoc, '=');

  const Expr *Init = VD->getInit();
  SourceRange ExprRange = Init->getSourceRange();
  SourceLocation InitEndLoc = ExprRange.getEnd();
  return !TheRewriter->RemoveText(SourceRange(InitStartLoc, InitEndLoc));
}

bool RewriteUtils::removeVarDecl(const VarDecl *VD,
                                 DeclGroupRef DGR)
{
  SourceRange VarRange = VD->getSourceRange();

  if (DGR.isSingleDecl()) {
    return !(TheRewriter->RemoveText(VarRange));
  }

  DeclGroupRef::const_iterator I = DGR.begin();
  const VarDecl *FirstVD = dyn_cast<VarDecl>(*I);
  // We cannot dyn_cast (*I) to VarDecl, because it could be a struct decl,
  // e.g., struct S1 { int f1; } s2 = {1}, where FirstDecl is
  // struct S1 {int f1;}. We need to skip it
  if (!FirstVD) {
    ++I;
    TransAssert((I != DGR.end()) && "Bad Decl!");
    FirstVD = dyn_cast<VarDecl>(*I);
    TransAssert(FirstVD && "Invalid Var Decl!");
    if (VD == FirstVD) {
      SourceLocation StartLoc = VD->getLocation();
      SourceLocation EndLoc = 
        getEndLocationUntil(VarRange, ',');

      return !(TheRewriter->RemoveText(SourceRange(StartLoc, EndLoc)));
    }
  }
  else if (VD == FirstVD) {
    SourceLocation StartLoc = getVarDeclTypeLocEnd(VD);

    SourceLocation EndLoc = 
      getEndLocationUntil(VarRange, ',');

    return !(TheRewriter->RemoveText(SourceRange(StartLoc, EndLoc)));
  }

  const VarDecl *PrevVD = FirstVD;
  const VarDecl *CurrVD = NULL;
  ++I;
  DeclGroupRef::const_iterator E = DGR.end();
  for (; I != E; ++I) {
    CurrVD = dyn_cast<VarDecl>(*I);
    TransAssert(CurrVD && "Not a valid VarDecl!");
    if (VD == CurrVD)
      break;
    PrevVD = CurrVD;
  }

  TransAssert((VD == CurrVD) && "Cannot find VD!");

  SourceLocation VarEndLoc = VarRange.getEnd();
  SourceRange PrevDeclRange = PrevVD->getSourceRange();

  SourceLocation PrevDeclEndLoc = 
    getEndLocationUntil(PrevDeclRange, ',');

  return !(TheRewriter->RemoveText(SourceRange(PrevDeclEndLoc, VarEndLoc)));
}

void RewriteUtils::getTmpTransName(unsigned Postfix, std::string &Name)
{
  std::stringstream SS;
  SS << getTmpVarNamePrefix() << Postfix;
  Name = SS.str();
}

bool RewriteUtils::insertStringBeforeFunc(const FunctionDecl *FD,
                                          const std::string &Str)
{
  SourceRange FuncRange = FD->getSourceRange();
  SourceLocation StartLoc = FuncRange.getBegin();
  return !TheRewriter->InsertTextBefore(StartLoc, Str);
}

bool RewriteUtils::replaceUnionWithStruct(const NamedDecl *ND)
{
  SourceRange NDRange = ND->getSourceRange();
  int RangeSize = TheRewriter->getRangeSize(NDRange);
  TransAssert((RangeSize != -1) && "Bad Range!");

  SourceLocation StartLoc = NDRange.getBegin();
  const char *StartBuf = SrcManager->getCharacterData(StartLoc);
  std::string TmpStr(StartBuf, RangeSize);
  std::string UStr = "union";
  size_t Pos = TmpStr.find(UStr);
  TransAssert((Pos != std::string::npos) && "Bad Name Position!");

  if (Pos != 0)
    StartLoc = StartLoc.getLocWithOffset(Pos);
  return !TheRewriter->ReplaceText(StartLoc, UStr.size(), "struct");
}

bool RewriteUtils::removeIfAndCond(const IfStmt *IS)
{
  SourceLocation IfLoc = IS->getIfLoc();
  const Stmt *ThenStmt = IS->getThen();
  TransAssert(ThenStmt && "NULL ThenStmt!");

  SourceLocation ThenLoc = ThenStmt->getLocStart();
  SourceLocation EndLoc =  ThenLoc.getLocWithOffset(-1);

  Rewriter::RewriteOptions Opts;
  // We don't want to include the previously inserted string
  Opts.IncludeInsertsAtBeginOfRange = false;
  return !TheRewriter->RemoveText(SourceRange(IfLoc, EndLoc), Opts);
}

bool RewriteUtils::removeArraySubscriptExpr(const Expr *E)
{
  SourceRange ERange = E->getSourceRange();
  SourceLocation StartLoc = ERange.getBegin();
  const char *StartBuf = SrcManager->getCharacterData(StartLoc);
  int Offset = 0;
  while (*StartBuf != '[') {
    StartBuf--;
    Offset--;
  }
  StartLoc = StartLoc.getLocWithOffset(Offset);

  SourceLocation EndLoc = ERange.getEnd();
  EndLoc = getLocationUntil(EndLoc, ']');
  return !TheRewriter->RemoveText(SourceRange(StartLoc, EndLoc));
}

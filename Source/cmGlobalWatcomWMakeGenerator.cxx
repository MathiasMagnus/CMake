/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#include "cmGlobalWatcomWMakeGenerator.h"
#include "cmLocalUnixMakefileGenerator3.h"
#include "cmMakefile.h"

cmGlobalWatcomWMakeGenerator::cmGlobalWatcomWMakeGenerator()
{
  this->FindMakeProgramFile = "CMakeFindWMake.cmake";
#ifdef _WIN32
  this->ForceUnixPaths = false;
#endif
  this->ToolSupportsColor = true;
  this->NeedSymbolicMark = true;
  this->EmptyRuleHackCommand = "@cd .";
#ifdef _WIN32
  this->WindowsShell = true;
#endif
  this->WatcomWMake = true;
  this->IncludeDirective = "!include";
}

void cmGlobalWatcomWMakeGenerator
::EnableLanguage(std::vector<std::string>const& l,
                 cmMakefile *mf,
                 bool optional)
{
  // pick a default
  mf->AddDefinition("WATCOM", "1");
  mf->AddDefinition("CMAKE_QUOTE_INCLUDE_PATHS", "1");
  mf->AddDefinition("CMAKE_MANGLE_OBJECT_FILE_NAMES", "1");
  mf->AddDefinition("CMAKE_MAKE_LINE_CONTINUE", "&");
  mf->AddDefinition("CMAKE_MAKE_SYMBOLIC_RULE", ".SYMBOLIC");
  mf->AddDefinition("CMAKE_GENERATOR_CC", "wcl386");
  mf->AddDefinition("CMAKE_GENERATOR_CXX", "wcl386");
  this->cmGlobalUnixMakefileGenerator3::EnableLanguage(l, mf, optional);
}

///! Create a local generator appropriate to this Global Generator
cmLocalGenerator *
cmGlobalWatcomWMakeGenerator::CreateLocalGenerator(cmLocalGenerator* parent)
{
  cmLocalUnixMakefileGenerator3* lg
      = new cmLocalUnixMakefileGenerator3(this, parent);
  lg->SetDefineWindowsNULL(true);
  lg->SetMakeSilentFlag("-h");
  lg->SetIgnoreLibPrefix(true);
  lg->SetPassMakeflags(false);
  lg->SetUnixCD(false);
  return lg;
}

//----------------------------------------------------------------------------
void cmGlobalWatcomWMakeGenerator
::GetDocumentation(cmDocumentationEntry& entry)
{
  entry.Name = cmGlobalWatcomWMakeGenerator::GetActualName();
  entry.Brief = "Generates Watcom WMake makefiles.";
}

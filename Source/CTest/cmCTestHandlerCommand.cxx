/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmCTestHandlerCommand.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include <cm/string_view>
#include <cmext/string_view>

#include "cmCTest.h"
#include "cmCTestGenericHandler.h"
#include "cmExecutionStatus.h"
#include "cmMakefile.h"
#include "cmMessageType.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"
#include "cmValue.h"
#include "cmWorkingDirectory.h"

namespace {
// class to save and restore the error state for ctest_* commands
// if a ctest_* command has a CAPTURE_CMAKE_ERROR then put the error
// state into there and restore the system wide error to what
// it was before the command ran
class SaveRestoreErrorState
{
public:
  SaveRestoreErrorState()
  {
    this->InitialErrorState = cmSystemTools::GetErrorOccurredFlag();
    cmSystemTools::ResetErrorOccurredFlag(); // rest the error state
    this->CaptureCMakeErrorValue = false;
  }
  // if the function has a CAPTURE_CMAKE_ERROR then we should restore
  // the error state to what it was before the function was run
  // if not then let the error state be what it is
  void CaptureCMakeError() { this->CaptureCMakeErrorValue = true; }
  ~SaveRestoreErrorState()
  {
    // if we are not saving the return value then make sure
    // if it was in error it goes back to being in error
    // otherwise leave it be what it is
    if (!this->CaptureCMakeErrorValue) {
      if (this->InitialErrorState) {
        cmSystemTools::SetErrorOccurred();
      }
      return;
    }
    // if we have saved the error in a return variable
    // then put things back exactly like they were
    bool currentState = cmSystemTools::GetErrorOccurredFlag();
    // if the state changed during this command we need
    // to handle it, if not then nothing needs to be done
    if (currentState != this->InitialErrorState) {
      // restore the initial error state
      if (this->InitialErrorState) {
        cmSystemTools::SetErrorOccurred();
      } else {
        cmSystemTools::ResetErrorOccurredFlag();
      }
    }
  }
  SaveRestoreErrorState(const SaveRestoreErrorState&) = delete;
  SaveRestoreErrorState& operator=(const SaveRestoreErrorState&) = delete;

private:
  bool InitialErrorState;
  bool CaptureCMakeErrorValue;
};
}

bool cmCTestHandlerCommand::InitialPass(
  std::vector<std::string> const& arguments, cmExecutionStatus& status)
{
  auto& args = *this;
  // save error state and restore it if needed
  SaveRestoreErrorState errorState;
  // Allocate space for argument values.
  this->BindArguments();

  // Process input arguments.
  std::vector<std::string> unparsedArguments;
  this->Parse(arguments, &unparsedArguments);
  this->CheckArguments();

  std::sort(args.ParsedKeywords.begin(), args.ParsedKeywords.end());
  auto it =
    std::adjacent_find(args.ParsedKeywords.begin(), args.ParsedKeywords.end());
  if (it != args.ParsedKeywords.end()) {
    this->Makefile->IssueMessage(
      MessageType::FATAL_ERROR,
      cmStrCat("Called with more than one value for ", *it));
  }

  bool const foundBadArgument = !unparsedArguments.empty();
  if (foundBadArgument) {
    this->SetError(cmStrCat("called with unknown argument \"",
                            unparsedArguments.front(), "\"."));
  }
  bool const captureCMakeError = !args.CaptureCMakeError.empty();
  // now that arguments are parsed check to see if there is a
  // CAPTURE_CMAKE_ERROR specified let the errorState object know.
  if (captureCMakeError) {
    errorState.CaptureCMakeError();
  }
  // if we found a bad argument then exit before running command
  if (foundBadArgument) {
    // store the cmake error
    if (captureCMakeError) {
      this->Makefile->AddDefinition(args.CaptureCMakeError, "-1");
      std::string const err = this->GetName() + " " + status.GetError();
      if (!cmSystemTools::FindLastString(err.c_str(), "unknown error.")) {
        cmCTestLog(this->CTest, ERROR_MESSAGE, err << " error from command\n");
      }
      // return success because failure is recorded in CAPTURE_CMAKE_ERROR
      return true;
    }
    // return failure because of bad argument
    return false;
  }

  // Set the config type of this ctest to the current value of the
  // CTEST_CONFIGURATION_TYPE script variable if it is defined.
  // The current script value trumps the -C argument on the command
  // line.
  cmValue ctestConfigType =
    this->Makefile->GetDefinition("CTEST_CONFIGURATION_TYPE");
  if (ctestConfigType) {
    this->CTest->SetConfigType(*ctestConfigType);
  }

  if (!args.Build.empty()) {
    this->CTest->SetCTestConfiguration(
      "BuildDirectory", cmSystemTools::CollapseFullPath(args.Build),
      args.Quiet);
  } else {
    std::string const& bdir =
      this->Makefile->GetSafeDefinition("CTEST_BINARY_DIRECTORY");
    if (!bdir.empty()) {
      this->CTest->SetCTestConfiguration(
        "BuildDirectory", cmSystemTools::CollapseFullPath(bdir), args.Quiet);
    } else {
      cmCTestLog(this->CTest, ERROR_MESSAGE,
                 "CTEST_BINARY_DIRECTORY not set" << std::endl);
    }
  }
  if (!args.Source.empty()) {
    cmCTestLog(this->CTest, DEBUG,
               "Set source directory to: " << args.Source << std::endl);
    this->CTest->SetCTestConfiguration(
      "SourceDirectory", cmSystemTools::CollapseFullPath(args.Source),
      args.Quiet);
  } else {
    this->CTest->SetCTestConfiguration(
      "SourceDirectory",
      cmSystemTools::CollapseFullPath(
        this->Makefile->GetSafeDefinition("CTEST_SOURCE_DIRECTORY")),
      args.Quiet);
  }

  if (cmValue changeId = this->Makefile->GetDefinition("CTEST_CHANGE_ID")) {
    this->CTest->SetCTestConfiguration("ChangeId", *changeId, args.Quiet);
  }

  cmCTestLog(this->CTest, DEBUG, "Initialize handler" << std::endl);
  auto handler = this->InitializeHandler();
  if (!handler) {
    cmCTestLog(this->CTest, ERROR_MESSAGE,
               "Cannot instantiate test handler " << this->GetName()
                                                  << std::endl);
    if (captureCMakeError) {
      this->Makefile->AddDefinition(args.CaptureCMakeError, "-1");
      std::string const& err = status.GetError();
      if (!cmSystemTools::FindLastString(err.c_str(), "unknown error.")) {
        cmCTestLog(this->CTest, ERROR_MESSAGE, err << " error from command\n");
      }
      return true;
    }
    return false;
  }

  handler->SetAppendXML(args.Append);

  handler->PopulateCustomVectors(this->Makefile);
  if (!args.SubmitIndex.empty()) {
    handler->SetSubmitIndex(atoi(args.SubmitIndex.c_str()));
  }
  cmWorkingDirectory workdir(
    this->CTest->GetCTestConfiguration("BuildDirectory"));
  if (workdir.Failed()) {
    this->SetError(workdir.GetError());
    if (captureCMakeError) {
      this->Makefile->AddDefinition(args.CaptureCMakeError, "-1");
      cmCTestLog(this->CTest, ERROR_MESSAGE,
                 this->GetName() << " " << status.GetError() << "\n");
      // return success because failure is recorded in CAPTURE_CMAKE_ERROR
      return true;
    }
    return false;
  }

  // reread time limit, as the variable may have been modified.
  this->CTest->SetTimeLimit(this->Makefile->GetDefinition("CTEST_TIME_LIMIT"));
  handler->SetCMakeInstance(this->Makefile->GetCMakeInstance());

  int res = handler->ProcessHandler();
  if (!args.ReturnValue.empty()) {
    this->Makefile->AddDefinition(args.ReturnValue, std::to_string(res));
  }
  this->ProcessAdditionalValues(handler.get());
  // log the error message if there was an error
  if (captureCMakeError) {
    const char* returnString = "0";
    if (cmSystemTools::GetErrorOccurredFlag()) {
      returnString = "-1";
      std::string const& err = status.GetError();
      // print out the error if it is not "unknown error" which means
      // there was no message
      if (!cmSystemTools::FindLastString(err.c_str(), "unknown error.")) {
        cmCTestLog(this->CTest, ERROR_MESSAGE, err);
      }
    }
    // store the captured cmake error state 0 or -1
    this->Makefile->AddDefinition(args.CaptureCMakeError, returnString);
  }
  return true;
}

void cmCTestHandlerCommand::BindArguments()
{
  this->BindParsedKeywords(this->ParsedKeywords);
  this->Bind("APPEND"_s, this->Append);
  this->Bind("QUIET"_s, this->Quiet);
  this->Bind("RETURN_VALUE"_s, this->ReturnValue);
  this->Bind("CAPTURE_CMAKE_ERROR"_s, this->CaptureCMakeError);
  this->Bind("SOURCE"_s, this->Source);
  this->Bind("BUILD"_s, this->Build);
  this->Bind("SUBMIT_INDEX"_s, this->SubmitIndex);
}

void cmCTestHandlerCommand::CheckArguments()
{
}

std::unique_ptr<cmCTestGenericHandler>
cmCTestHandlerCommand::InitializeHandler()
{
  return nullptr;
};

void cmCTestHandlerCommand::ProcessAdditionalValues(cmCTestGenericHandler*)
{
}

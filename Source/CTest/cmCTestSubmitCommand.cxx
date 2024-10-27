/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmCTestSubmitCommand.h"

#include <set>
#include <sstream>
#include <utility>

#include <cm/memory>
#include <cm/vector>
#include <cmext/string_view>

#include "cmArgumentParser.h"
#include "cmCTest.h"
#include "cmCTestGenericHandler.h"
#include "cmCTestSubmitHandler.h"
#include "cmCommand.h"
#include "cmList.h"
#include "cmMakefile.h"
#include "cmMessageType.h"
#include "cmRange.h"
#include "cmSystemTools.h"
#include "cmValue.h"

class cmExecutionStatus;

std::unique_ptr<cmCommand> cmCTestSubmitCommand::Clone()
{
  auto ni = cm::make_unique<cmCTestSubmitCommand>();
  ni->CTest = this->CTest;
  return std::unique_ptr<cmCommand>(std::move(ni));
}

std::unique_ptr<cmCTestGenericHandler> cmCTestSubmitCommand::InitializeHandler(
  HandlerArguments& arguments)
{
  auto const& args = static_cast<SubmitArguments&>(arguments);
  cmValue submitURL = !args.SubmitURL.empty()
    ? cmValue(args.SubmitURL)
    : this->Makefile->GetDefinition("CTEST_SUBMIT_URL");

  if (submitURL) {
    this->CTest->SetCTestConfiguration("SubmitURL", *submitURL, args.Quiet);
  } else {
    this->CTest->SetCTestConfigurationFromCMakeVariable(
      this->Makefile, "DropMethod", "CTEST_DROP_METHOD", args.Quiet);
    this->CTest->SetCTestConfigurationFromCMakeVariable(
      this->Makefile, "DropSiteUser", "CTEST_DROP_SITE_USER", args.Quiet);
    this->CTest->SetCTestConfigurationFromCMakeVariable(
      this->Makefile, "DropSitePassword", "CTEST_DROP_SITE_PASSWORD",
      args.Quiet);
    this->CTest->SetCTestConfigurationFromCMakeVariable(
      this->Makefile, "DropSite", "CTEST_DROP_SITE", args.Quiet);
    this->CTest->SetCTestConfigurationFromCMakeVariable(
      this->Makefile, "DropLocation", "CTEST_DROP_LOCATION", args.Quiet);
  }

  if (!this->CTest->SetCTestConfigurationFromCMakeVariable(
        this->Makefile, "TLSVersion", "CTEST_TLS_VERSION", args.Quiet)) {
    if (cmValue tlsVersionVar =
          this->Makefile->GetDefinition("CMAKE_TLS_VERSION")) {
      cmCTestOptionalLog(
        this->CTest, HANDLER_VERBOSE_OUTPUT,
        "SetCTestConfiguration from CMAKE_TLS_VERSION:TLSVersion:"
          << *tlsVersionVar << std::endl,
        args.Quiet);
      this->CTest->SetCTestConfiguration("TLSVersion", *tlsVersionVar,
                                         args.Quiet);
    } else if (cm::optional<std::string> tlsVersionEnv =
                 cmSystemTools::GetEnvVar("CMAKE_TLS_VERSION")) {
      cmCTestOptionalLog(
        this->CTest, HANDLER_VERBOSE_OUTPUT,
        "SetCTestConfiguration from ENV{CMAKE_TLS_VERSION}:TLSVersion:"
          << *tlsVersionEnv << std::endl,
        args.Quiet);
      this->CTest->SetCTestConfiguration("TLSVersion", *tlsVersionEnv,
                                         args.Quiet);
    }
  }
  if (!this->CTest->SetCTestConfigurationFromCMakeVariable(
        this->Makefile, "TLSVerify", "CTEST_TLS_VERIFY", args.Quiet)) {
    if (cmValue tlsVerifyVar =
          this->Makefile->GetDefinition("CMAKE_TLS_VERIFY")) {
      cmCTestOptionalLog(
        this->CTest, HANDLER_VERBOSE_OUTPUT,
        "SetCTestConfiguration from CMAKE_TLS_VERIFY:TLSVerify:"
          << *tlsVerifyVar << std::endl,
        args.Quiet);
      this->CTest->SetCTestConfiguration("TLSVerify", *tlsVerifyVar,
                                         args.Quiet);
    } else if (cm::optional<std::string> tlsVerifyEnv =
                 cmSystemTools::GetEnvVar("CMAKE_TLS_VERIFY")) {
      cmCTestOptionalLog(
        this->CTest, HANDLER_VERBOSE_OUTPUT,
        "SetCTestConfiguration from ENV{CMAKE_TLS_VERIFY}:TLSVerify:"
          << *tlsVerifyEnv << std::endl,
        args.Quiet);
      this->CTest->SetCTestConfiguration("TLSVerify", *tlsVerifyEnv,
                                         args.Quiet);
    }
  }
  this->CTest->SetCTestConfigurationFromCMakeVariable(
    this->Makefile, "CurlOptions", "CTEST_CURL_OPTIONS", args.Quiet);
  this->CTest->SetCTestConfigurationFromCMakeVariable(
    this->Makefile, "SubmitInactivityTimeout",
    "CTEST_SUBMIT_INACTIVITY_TIMEOUT", args.Quiet);

  cmValue notesFilesVariable =
    this->Makefile->GetDefinition("CTEST_NOTES_FILES");
  if (notesFilesVariable) {
    cmList notesFiles{ *notesFilesVariable };
    this->CTest->GenerateNotesFile(this->Makefile->GetCMakeInstance(),
                                   notesFiles);
  }

  cmValue extraFilesVariable =
    this->Makefile->GetDefinition("CTEST_EXTRA_SUBMIT_FILES");
  if (extraFilesVariable) {
    cmList extraFiles{ *extraFilesVariable };
    if (!this->CTest->SubmitExtraFiles(extraFiles)) {
      this->SetError("problem submitting extra files.");
      return nullptr;
    }
  }

  auto handler = cm::make_unique<cmCTestSubmitHandler>(this->CTest);

  // If no FILES or PARTS given, *all* PARTS are submitted by default.
  //
  // If FILES are given, but not PARTS, only the FILES are submitted
  // and *no* PARTS are submitted.
  //  (This is why we select the empty "noParts" set in the
  //   if(args.Files) block below...)
  //
  // If PARTS are given, only the selected PARTS are submitted.
  //
  // If both PARTS and FILES are given, only the selected PARTS *and*
  // all the given FILES are submitted.

  // If given explicit FILES to submit, pass them to the handler.
  //
  if (args.Files) {
    // Intentionally select *no* PARTS. (Pass an empty set.) If PARTS
    // were also explicitly mentioned, they will be selected below...
    // But FILES with no PARTS mentioned should just submit the FILES
    // without any of the default parts.
    //
    handler->SelectParts(std::set<cmCTest::Part>());
    handler->SelectFiles(
      std::set<std::string>(args.Files->begin(), args.Files->end()));
  }

  // If a PARTS option was given, select only the named parts for submission.
  //
  if (args.Parts) {
    auto parts =
      cmMakeRange(*(args.Parts)).transform([this](std::string const& arg) {
        return this->CTest->GetPartFromName(arg);
      });
    handler->SelectParts(std::set<cmCTest::Part>(parts.begin(), parts.end()));
  }

  // Pass along any HTTPHEADER to the handler if this option was given.
  if (!args.HttpHeaders.empty()) {
    handler->SetHttpHeaders(args.HttpHeaders);
  }

  handler->RetryDelay = args.RetryDelay;
  handler->RetryCount = args.RetryCount;
  handler->InternalTest = args.InternalTest;

  handler->SetQuiet(args.Quiet);

  if (args.CDashUpload) {
    handler->CDashUpload = true;
    handler->CDashUploadFile = args.CDashUploadFile;
    handler->CDashUploadType = args.CDashUploadType;
  }
  return std::unique_ptr<cmCTestGenericHandler>(std::move(handler));
}

bool cmCTestSubmitCommand::InitialPass(std::vector<std::string> const& args,
                                       cmExecutionStatus& status)
{
  // Arguments used by both modes.
  static auto const parserBase =
    cmArgumentParser<SubmitArguments>{ MakeHandlerParser<SubmitArguments>() }
      .Bind("BUILD_ID"_s, &SubmitArguments::BuildID)
      .Bind("HTTPHEADER"_s, &SubmitArguments::HttpHeaders)
      .Bind("RETRY_COUNT"_s, &SubmitArguments::RetryCount)
      .Bind("RETRY_DELAY"_s, &SubmitArguments::RetryDelay)
      .Bind("SUBMIT_URL"_s, &SubmitArguments::SubmitURL)
      .Bind("INTERNAL_TEST_CHECKSUM"_s, &SubmitArguments::InternalTest);

  // Arguments specific to the CDASH_UPLOAD signature.
  static auto const uploadParser =
    cmArgumentParser<SubmitArguments>{ parserBase }
      .Bind("CDASH_UPLOAD"_s, &SubmitArguments::CDashUploadFile)
      .Bind("CDASH_UPLOAD_TYPE"_s, &SubmitArguments::CDashUploadType);

  // Arguments that cannot be used with CDASH_UPLOAD.
  static auto const partsParser =
    cmArgumentParser<SubmitArguments>{ parserBase }
      .Bind("PARTS"_s, &SubmitArguments::Parts)
      .Bind("FILES"_s, &SubmitArguments::Files);

  bool const cdashUpload = !args.empty() && args[0] == "CDASH_UPLOAD";
  auto const& parser = cdashUpload ? uploadParser : partsParser;

  std::vector<std::string> unparsedArguments;
  SubmitArguments arguments = parser.Parse(args, &unparsedArguments);
  arguments.CDashUpload = cdashUpload;
  return this->ExecuteHandlerCommand(arguments, unparsedArguments, status);
}

void cmCTestSubmitCommand::CheckArguments(HandlerArguments& arguments)
{
  auto& args = static_cast<SubmitArguments&>(arguments);
  if (args.Parts) {
    cm::erase_if(*(args.Parts), [this](std::string const& arg) -> bool {
      cmCTest::Part p = this->CTest->GetPartFromName(arg);
      if (p == cmCTest::PartCount) {
        std::ostringstream e;
        e << "Part name \"" << arg << "\" is invalid.";
        this->Makefile->IssueMessage(MessageType::FATAL_ERROR, e.str());
        return true;
      }
      return false;
    });
  }

  if (args.Files) {
    cm::erase_if(*(args.Files), [this](std::string const& arg) -> bool {
      if (!cmSystemTools::FileExists(arg)) {
        std::ostringstream e;
        e << "File \"" << arg << "\" does not exist. Cannot submit "
          << "a non-existent file.";
        this->Makefile->IssueMessage(MessageType::FATAL_ERROR, e.str());
        return true;
      }
      return false;
    });
  }
}

void cmCTestSubmitCommand::ProcessAdditionalValues(
  cmCTestGenericHandler*, HandlerArguments const& arguments)
{
  auto const& args = static_cast<SubmitArguments const&>(arguments);
  if (!args.BuildID.empty()) {
    this->Makefile->AddDefinition(args.BuildID, this->CTest->GetBuildID());
  }
}

#include <iostream>
#include <unistd.h>

#include "PWScore.h"
#include "os/file.h"

using namespace std;

static void usage(char *pname)
{
  cerr << "Usage: " << pname << " safe passkey" << endl;
}

static const char *status_text(int status)
{
  switch (status) {
  case PWScore::SUCCESS: return "SUCCESS";
  case PWScore::FAILURE: return "FAILURE";
  case PWScore::CANT_OPEN_FILE: return "CANT_OPEN_FILE";
  case PWScore::USER_CANCEL: return "USER_CANCEL";
  case PWScore::WRONG_PASSWORD: return "WRONG_PASSWORD";
  case PWScore::BAD_DIGEST: return "BAD_DIGEST";
  case PWScore::UNKNOWN_VERSION: return "UNKNOWN_VERSION";
  case PWScore::NOT_SUCCESS: return "NOT_SUCCESS";
  case PWScore::ALREADY_OPEN: return "ALREADY_OPEN";
  case PWScore::INVALID_FORMAT: return "INVALID_FORMAT";
  case PWScore::USER_EXIT: return "USER_EXIT";
  case PWScore::XML_FAILED_VALIDATION: return "XML_FAILED_VALIDATION";
  case PWScore::XML_FAILED_IMPORT: return "XML_FAILED_IMPORT";
  case PWScore::LIMIT_REACHED: return "LIMIT_REACHED";
  case PWScore::UNIMPLEMENTED: return "UNIMPLEMENTED";
  default: return "UNKNOWN !!!";
  }
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    usage(argv[0]);
    return 1;
  }

  PWScore core;
  if (!pws_os::FileExists(argv[1])) {
    cerr << argv[1] << " - file not found" << endl;
    return 2;
  }
  int status;
  status = core.CheckPassword(argv[1], argv[2]);
  cout << "CheckPassword returned: " << status_text(status) << endl;
  if (status != PWScore::SUCCESS)
    goto done;
  {
    stringT locker(getlogin() != NULL ? getlogin() : "unknown");
    if (!core.LockFile(argv[1], locker)) {
      cout << "Couldn't lock file " << locker << endl;
      status = -1;
      goto done;
    }
  }
  status = core.ReadFile(argv[1], argv[2]);
  cout << "ReadFile returned: " << status_text(status) << endl;
  if (status != PWScore::SUCCESS)
    goto done;
  cout << argv[1] << " has " << core.GetNumEntries() << " entries" << endl;
  {
    CItemData::FieldBits bits(~0L);
    for (ItemListConstIter iter = core.GetEntryIter();
         iter != core.GetEntryEndIter(); iter++) {
      StringX text = iter->second.GetPlaintext('|', bits, '-', NULL);
      cout << text << endl;
    }
  }
 done:
  core.UnlockFile(argv[1]);
  return status;
}
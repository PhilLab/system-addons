/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Brian Nesse <bnesse@netscape.com>
 *   Frederic Plourde <frederic.plourde@collabora.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"

#include "nsPrefBranch.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDirectoryService.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsIStringBundle.h"
#include "prefapi.h"
#include "prmem.h"
#include "pldhash.h"

#include "plstr.h"
#include "nsCRT.h"
#include "mozilla/Services.h"

#include "prefapi_private_data.h"

// Definitions
struct EnumerateData {
  const char  *parent;
  nsTArray<nsCString> *pref_list;
};

// Prototypes
static PLDHashOperator
  pref_enumChild(PLDHashTable *table, PLDHashEntryHdr *heh,
                 PRUint32 i, void *arg);

using mozilla::dom::ContentChild;

static ContentChild*
GetContentChild()
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild* cpc = ContentChild::GetSingleton();
    if (!cpc) {
      NS_RUNTIMEABORT("Content Protocol is NULL!  We're going to crash!");
    }
    return cpc;
  }
  return nsnull;
}

/*
 * Constructor/Destructor
 */

nsPrefBranch::nsPrefBranch(const char *aPrefRoot, bool aDefaultBranch)
{
  mPrefRoot = aPrefRoot;
  mPrefRootLength = mPrefRoot.Length();
  mIsDefault = aDefaultBranch;
  mFreeingObserverList = false;
  mObservers.Init();

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    ++mRefCnt;    // Our refcnt must be > 0 when we call this, or we'll get deleted!
    // add weak so we don't have to clean up at shutdown
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    --mRefCnt;
  }
}

nsPrefBranch::~nsPrefBranch()
{
  freeObserverList();

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService)
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
}


/*
 * nsISupports Implementation
 */

NS_IMPL_THREADSAFE_ADDREF(nsPrefBranch)
NS_IMPL_THREADSAFE_RELEASE(nsPrefBranch)

NS_INTERFACE_MAP_BEGIN(nsPrefBranch)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIPrefBranch2, !mIsDefault)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIPrefBranchInternal, !mIsDefault)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


/*
 * nsIPrefBranch Implementation
 */

NS_IMETHODIMP nsPrefBranch::GetRoot(char **aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);
  mPrefRoot.Truncate(mPrefRootLength);
  *aRoot = ToNewCString(mPrefRoot);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetPrefType(const char *aPrefName, PRInt32 *_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  *_retval = PREF_GetPrefType(pref);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::GetBoolPref(const char *aPrefName, bool *_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_GetBoolPref(pref, _retval, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::SetBoolPref(const char *aPrefName, bool aValue)
{
  if (GetContentChild()) {
    NS_ERROR("cannot set pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_SetBoolPref(pref, aValue, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::GetCharPref(const char *aPrefName, char **_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_CopyCharPref(pref, _retval, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::SetCharPref(const char *aPrefName, const char *aValue)
{
  if (GetContentChild()) {
    NS_ERROR("cannot set pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aPrefName);
  NS_ENSURE_ARG(aValue);
  const char *pref = getPrefName(aPrefName);
  return PREF_SetCharPref(pref, aValue, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::GetIntPref(const char *aPrefName, PRInt32 *_retval)
{
  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_GetIntPref(pref, _retval, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::SetIntPref(const char *aPrefName, PRInt32 aValue)
{
  if (GetContentChild()) {
    NS_ERROR("cannot set pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_SetIntPref(pref, aValue, mIsDefault);
}

NS_IMETHODIMP nsPrefBranch::GetComplexValue(const char *aPrefName, const nsIID & aType, void **_retval)
{
  NS_ENSURE_ARG(aPrefName);

  nsresult       rv;
  nsXPIDLCString utf8String;

  // we have to do this one first because it's different than all the rest
  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString(do_CreateInstance(NS_PREFLOCALIZEDSTRING_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    const char *pref = getPrefName(aPrefName);
    bool    bNeedDefault = false;

    if (mIsDefault) {
      bNeedDefault = true;
    } else {
      // if there is no user (or locked) value
      if (!PREF_HasUserPref(pref) && !PREF_PrefIsLocked(pref)) {
        bNeedDefault = true;
      }
    }

    // if we need to fetch the default value, do that instead, otherwise use the
    // value we pulled in at the top of this function
    if (bNeedDefault) {
      nsXPIDLString utf16String;
      rv = GetDefaultFromPropertiesFile(pref, getter_Copies(utf16String));
      if (NS_SUCCEEDED(rv)) {
        theString->SetData(utf16String.get());
      }
    } else {
      rv = GetCharPref(aPrefName, getter_Copies(utf8String));
      if (NS_SUCCEEDED(rv)) {
        theString->SetData(NS_ConvertUTF8toUTF16(utf8String).get());
      }
    }

    if (NS_SUCCEEDED(rv)) {
      theString.forget(reinterpret_cast<nsIPrefLocalizedString**>(_retval));
    }

    return rv;
  }

  // if we can't get the pref, there's no point in being here
  rv = GetCharPref(aPrefName, getter_Copies(utf8String));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsILocalFile))) {
    if (GetContentChild()) {
      NS_ERROR("cannot get nsILocalFile pref from content process");
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = file->SetPersistentDescriptor(utf8String);
      if (NS_SUCCEEDED(rv)) {
        file.forget(reinterpret_cast<nsILocalFile**>(_retval));
        return NS_OK;
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    if (GetContentChild()) {
      NS_ERROR("cannot get nsIRelativeFilePref from content process");
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsACString::const_iterator keyBegin, strEnd;
    utf8String.BeginReading(keyBegin);
    utf8String.EndReading(strEnd);    

    // The pref has the format: [fromKey]a/b/c
    if (*keyBegin++ != '[')        
      return NS_ERROR_FAILURE;
    nsACString::const_iterator keyEnd(keyBegin);
    if (!FindCharInReadable(']', keyEnd, strEnd))
      return NS_ERROR_FAILURE;
    nsCAutoString key(Substring(keyBegin, keyEnd));
    
    nsCOMPtr<nsILocalFile> fromFile;        
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = directoryService->Get(key.get(), NS_GET_IID(nsILocalFile), getter_AddRefs(fromFile));
    if (NS_FAILED(rv))
      return rv;
    
    nsCOMPtr<nsILocalFile> theFile;
    rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(theFile));
    if (NS_FAILED(rv))
      return rv;
    rv = theFile->SetRelativeDescriptor(fromFile, Substring(++keyEnd, strEnd));
    if (NS_FAILED(rv))
      return rv;
    nsCOMPtr<nsIRelativeFilePref> relativePref;
    rv = NS_NewRelativeFilePref(theFile, key, getter_AddRefs(relativePref));
    if (NS_FAILED(rv))
      return rv;

    relativePref.forget(reinterpret_cast<nsIRelativeFilePref**>(_retval));
    return NS_OK;
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
    nsCOMPtr<nsISupportsString> theString(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      theString->SetData(NS_ConvertUTF8toUTF16(utf8String));
      theString.forget(reinterpret_cast<nsISupportsString**>(_retval));
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::GetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPrefBranch::SetComplexValue(const char *aPrefName, const nsIID & aType, nsISupports *aValue)
{
  if (GetContentChild()) {
    NS_ERROR("cannot set pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aPrefName);

  nsresult   rv = NS_NOINTERFACE;

  if (aType.Equals(NS_GET_IID(nsILocalFile))) {
    nsCOMPtr<nsILocalFile> file = do_QueryInterface(aValue);
    if (!file)
      return NS_NOINTERFACE;
    nsCAutoString descriptorString;

    rv = file->GetPersistentDescriptor(descriptorString);
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPref(aPrefName, descriptorString.get());
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    nsCOMPtr<nsIRelativeFilePref> relFilePref = do_QueryInterface(aValue);
    if (!relFilePref)
      return NS_NOINTERFACE;
    
    nsCOMPtr<nsILocalFile> file;
    relFilePref->GetFile(getter_AddRefs(file));
    if (!file)
      return NS_NOINTERFACE;
    nsCAutoString relativeToKey;
    (void) relFilePref->GetRelativeToKey(relativeToKey);

    nsCOMPtr<nsILocalFile> relativeToFile;        
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = directoryService->Get(relativeToKey.get(), NS_GET_IID(nsILocalFile), getter_AddRefs(relativeToFile));
    if (NS_FAILED(rv))
      return rv;

    nsCAutoString relDescriptor;
    rv = file->GetRelativeDescriptor(relativeToFile, relDescriptor);
    if (NS_FAILED(rv))
      return rv;
    
    nsCAutoString descriptorString;
    descriptorString.Append('[');
    descriptorString.Append(relativeToKey);
    descriptorString.Append(']');
    descriptorString.Append(relDescriptor);
    return SetCharPref(aPrefName, descriptorString.get());
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
    nsCOMPtr<nsISupportsString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsAutoString wideString;

      rv = theString->GetData(wideString);
      if (NS_SUCCEEDED(rv)) {
        rv = SetCharPref(aPrefName, NS_ConvertUTF16toUTF8(wideString).get());
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsXPIDLString wideString;

      rv = theString->GetData(getter_Copies(wideString));
      if (NS_SUCCEEDED(rv)) {
        rv = SetCharPref(aPrefName, NS_ConvertUTF16toUTF8(wideString).get());
      }
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::SetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPrefBranch::ClearUserPref(const char *aPrefName)
{
  if (GetContentChild()) {
    NS_ERROR("cannot set pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_ClearUserPref(pref);
}

NS_IMETHODIMP nsPrefBranch::PrefHasUserValue(const char *aPrefName, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  *_retval = PREF_HasUserPref(pref);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::LockPref(const char *aPrefName)
{
  if (GetContentChild()) {
    NS_ERROR("cannot lock pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_LockPref(pref, true);
}

NS_IMETHODIMP nsPrefBranch::PrefIsLocked(const char *aPrefName, bool *_retval)
{
  if (GetContentChild()) {
    NS_ERROR("cannot check lock pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  *_retval = PREF_PrefIsLocked(pref);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::UnlockPref(const char *aPrefName)
{
  if (GetContentChild()) {
    NS_ERROR("cannot unlock pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aPrefName);
  const char *pref = getPrefName(aPrefName);
  return PREF_LockPref(pref, false);
}

/* void resetBranch (in string startingAt); */
NS_IMETHODIMP nsPrefBranch::ResetBranch(const char *aStartingAt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPrefBranch::DeleteBranch(const char *aStartingAt)
{
  if (GetContentChild()) {
    NS_ERROR("cannot set pref from content process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_ARG(aStartingAt);
  const char *pref = getPrefName(aStartingAt);
  return PREF_DeleteBranch(pref);
}

NS_IMETHODIMP nsPrefBranch::GetChildList(const char *aStartingAt, PRUint32 *aCount, char ***aChildArray)
{
  char            **outArray;
  PRInt32         numPrefs;
  PRInt32         dwIndex;
  EnumerateData   ed;
  nsAutoTArray<nsCString, 32> prefArray;

  NS_ENSURE_ARG(aStartingAt);
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aChildArray);

  *aChildArray = nsnull;
  *aCount = 0;

  if (!gHashTable.ops)
    return NS_ERROR_NOT_INITIALIZED;

  // this will contain a list of all the pref name strings
  // allocate on the stack for speed
  
  ed.parent = getPrefName(aStartingAt);
  ed.pref_list = &prefArray;
  PL_DHashTableEnumerate(&gHashTable, pref_enumChild, &ed);

  // now that we've built up the list, run the callback on
  // all the matching elements
  numPrefs = prefArray.Length();

  if (numPrefs) {
    outArray = (char **)nsMemory::Alloc(numPrefs * sizeof(char *));
    if (!outArray)
      return NS_ERROR_OUT_OF_MEMORY;

    for (dwIndex = 0; dwIndex < numPrefs; ++dwIndex) {
      // we need to lop off mPrefRoot in case the user is planning to pass this
      // back to us because if they do we are going to add mPrefRoot again.
      const nsCString& element = prefArray[dwIndex];
      outArray[dwIndex] = (char *)nsMemory::Clone(
        element.get() + mPrefRootLength, element.Length() - mPrefRootLength + 1);

      if (!outArray[dwIndex]) {
        // we ran out of memory... this is annoying
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(dwIndex, outArray);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    *aChildArray = outArray;
  }
  *aCount = numPrefs;

  return NS_OK;
}


/*
 *  nsIPrefBranch2 methods
 */

NS_IMETHODIMP nsPrefBranch::AddObserver(const char *aDomain, nsIObserver *aObserver, bool aHoldWeak)
{
  PrefCallback *pCallback;
  const char *pref;

  NS_ENSURE_ARG(aDomain);
  NS_ENSURE_ARG(aObserver);

  // hold a weak reference to the observer if so requested
  if (aHoldWeak) {
    nsCOMPtr<nsISupportsWeakReference> weakRefFactory = do_QueryInterface(aObserver);
    if (!weakRefFactory) {
      // the caller didn't give us a object that supports weak reference... tell them
      return NS_ERROR_INVALID_ARG;
    }

    // Construct a PrefCallback with a weak reference to the observer.
    pCallback = new PrefCallback(aDomain, weakRefFactory, this);

  } else {
    // Construct a PrefCallback with a strong reference to the observer.
    pCallback = new PrefCallback(aDomain, aObserver, this);
  }

  if (mObservers.Get(pCallback)) {
    NS_WARNING("Ignoring duplicate observer.");
    delete pCallback;
    return NS_OK;
  }

  bool putSucceeded = mObservers.Put(pCallback, pCallback);

  if (!putSucceeded) {
    delete pCallback;
    return NS_ERROR_FAILURE;
  }

  // We must pass a fully qualified preference name to the callback
  // aDomain == nsnull is the only possible failure, and we trapped it with
  // NS_ENSURE_ARG above.
  pref = getPrefName(aDomain);
  PREF_RegisterCallback(pref, NotifyObserver, pCallback);
  return NS_OK;
}

NS_IMETHODIMP nsPrefBranch::RemoveObserver(const char *aDomain, nsIObserver *aObserver)
{
  NS_ENSURE_ARG(aDomain);
  NS_ENSURE_ARG(aObserver);

  nsresult rv = NS_OK;

  // If we're in the middle of a call to freeObserverList, don't process this
  // RemoveObserver call -- the observer in question will be removed soon, if
  // it hasn't been already.
  //
  // It's important that we don't touch mObservers in any way -- even a Get()
  // which retuns null might cause the hashtable to resize itself, which will
  // break the Enumerator in freeObserverList.
  if (mFreeingObserverList)
    return NS_OK;

  // Remove the relevant PrefCallback from mObservers and get an owning
  // pointer to it.  Unregister the callback first, and then let the owning
  // pointer go out of scope and destroy the callback.
  PrefCallback key(aDomain, aObserver, this);
  nsAutoPtr<PrefCallback> pCallback;
  mObservers.RemoveAndForget(&key, pCallback);
  if (pCallback) {
    // aDomain == nsnull is the only possible failure, trapped above
    const char *pref = getPrefName(aDomain);
    rv = PREF_UnregisterCallback(pref, NotifyObserver, pCallback);
  }

  return rv;
}

NS_IMETHODIMP nsPrefBranch::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  // watch for xpcom shutdown and free our observers to eliminate any cyclic references
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    freeObserverList();
  }
  return NS_OK;
}

/* static */
nsresult nsPrefBranch::NotifyObserver(const char *newpref, void *data)
{
  PrefCallback *pCallback = (PrefCallback *)data;

  nsCOMPtr<nsIObserver> observer = pCallback->GetObserver();
  if (!observer) {
    // The observer has expired.  Let's remove this callback.
    pCallback->GetPrefBranch()->RemoveExpiredCallback(pCallback);
    return NS_OK;
  }

  // remove any root this string may contain so as to not confuse the observer
  // by passing them something other than what they passed us as a topic
  PRUint32 len = pCallback->GetPrefBranch()->GetRootLength();
  nsCAutoString suffix(newpref + len);

  observer->Observe(static_cast<nsIPrefBranch *>(pCallback->GetPrefBranch()),
                    NS_PREFBRANCH_PREFCHANGE_TOPIC_ID,
                    NS_ConvertASCIItoUTF16(suffix).get());
  return NS_OK;
}

PLDHashOperator
FreeObserverFunc(PrefCallback *aKey,
                 nsAutoPtr<PrefCallback> &aCallback,
                 void *aArgs)
{
  // Calling NS_RELEASE below might trigger a call to
  // nsPrefBranch::RemoveObserver, since some classes remove themselves from
  // the pref branch on destruction.  We don't need to worry about this causing
  // double-frees, however, because freeObserverList sets mFreeingObserverList
  // to true, which prevents RemoveObserver calls from doing anything.

  nsPrefBranch *prefBranch = aCallback->GetPrefBranch();
  const char *pref = prefBranch->getPrefName(aCallback->GetDomain().get());
  PREF_UnregisterCallback(pref, nsPrefBranch::NotifyObserver, aCallback);

  return PL_DHASH_REMOVE;
}

void nsPrefBranch::freeObserverList(void)
{
  // We need to prevent anyone from modifying mObservers while we're
  // enumerating over it.  In particular, some clients will call
  // RemoveObserver() when they're destructed; we need to keep those calls from
  // touching mObservers.
  mFreeingObserverList = true;
  mObservers.Enumerate(&FreeObserverFunc, nsnull);
  mFreeingObserverList = false;
}

void
nsPrefBranch::RemoveExpiredCallback(PrefCallback *aCallback)
{
  NS_PRECONDITION(aCallback->IsExpired(), "Callback should be expired.");
  mObservers.Remove(aCallback);
}

nsresult nsPrefBranch::GetDefaultFromPropertiesFile(const char *aPrefName, PRUnichar **return_buf)
{
  nsresult rv;

  // the default value contains a URL to a .properties file
    
  nsXPIDLCString propertyFileURL;
  rv = PREF_CopyCharPref(aPrefName, getter_Copies(propertyFileURL), true);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (!bundleService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(propertyFileURL,
                                   getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return rv;

  // string names are in unicode
  nsAutoString stringId;
  stringId.AssignASCII(aPrefName);

  return bundle->GetStringFromName(stringId.get(), return_buf);
}

const char *nsPrefBranch::getPrefName(const char *aPrefName)
{
  NS_ASSERTION(aPrefName, "null pref name!");

  // for speed, avoid strcpy if we can:
  if (mPrefRoot.IsEmpty())
    return aPrefName;

  // isn't there a better way to do this? this is really kind of gross.
  mPrefRoot.Truncate(mPrefRootLength);
  mPrefRoot.Append(aPrefName);
  return mPrefRoot.get();
}

static PLDHashOperator
pref_enumChild(PLDHashTable *table, PLDHashEntryHdr *heh,
               PRUint32 i, void *arg)
{
  PrefHashEntry *he = static_cast<PrefHashEntry*>(heh);
  EnumerateData *d = reinterpret_cast<EnumerateData *>(arg);
  if (strncmp(he->key, d->parent, strlen(d->parent)) == 0) {
    d->pref_list->AppendElement(he->key);
  }
  return PL_DHASH_NEXT;
}

//----------------------------------------------------------------------------
// nsPrefLocalizedString
//----------------------------------------------------------------------------

nsPrefLocalizedString::nsPrefLocalizedString()
{
}

nsPrefLocalizedString::~nsPrefLocalizedString()
{
}


/*
 * nsISupports Implementation
 */

NS_IMPL_THREADSAFE_ADDREF(nsPrefLocalizedString)
NS_IMPL_THREADSAFE_RELEASE(nsPrefLocalizedString)

NS_INTERFACE_MAP_BEGIN(nsPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY(nsIPrefLocalizedString)
    NS_INTERFACE_MAP_ENTRY(nsISupportsString)
NS_INTERFACE_MAP_END

nsresult nsPrefLocalizedString::Init()
{
  nsresult rv;
  mUnicodeString = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);

  return rv;
}

NS_IMETHODIMP
nsPrefLocalizedString::GetData(PRUnichar **_retval)
{
  nsAutoString data;

  nsresult rv = GetData(data);
  if (NS_FAILED(rv))
    return rv;
  
  *_retval = ToNewUnicode(data);
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsPrefLocalizedString::SetData(const PRUnichar *aData)
{
  if (!aData)
    return SetData(EmptyString());
  return SetData(nsDependentString(aData));
}

NS_IMETHODIMP
nsPrefLocalizedString::SetDataWithLength(PRUint32 aLength,
                                         const PRUnichar *aData)
{
  if (!aData)
    return SetData(EmptyString());
  return SetData(Substring(aData, aData + aLength));
}

//----------------------------------------------------------------------------
// nsRelativeFilePref
//----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRelativeFilePref, nsIRelativeFilePref)

nsRelativeFilePref::nsRelativeFilePref()
{
}

nsRelativeFilePref::~nsRelativeFilePref()
{
}

NS_IMETHODIMP nsRelativeFilePref::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = mFile;
  NS_IF_ADDREF(*aFile);
  return NS_OK;
}

NS_IMETHODIMP nsRelativeFilePref::SetFile(nsILocalFile *aFile)
{
  mFile = aFile;
  return NS_OK;
}

NS_IMETHODIMP nsRelativeFilePref::GetRelativeToKey(nsACString& aRelativeToKey)
{
  aRelativeToKey.Assign(mRelativeToKey);
  return NS_OK;
}

NS_IMETHODIMP nsRelativeFilePref::SetRelativeToKey(const nsACString& aRelativeToKey)
{
  mRelativeToKey.Assign(aRelativeToKey);
  return NS_OK;
}

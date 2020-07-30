// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "DefaultSecurityDescriptor.h"

DefaultSecurityDescriptor::DefaultSecurityDescriptor(DWORD userPermissions, DWORD adminPermissions) :
  m_pInteractiveSID(nullptr),
  m_pLocalUsersSID(nullptr),
  m_pAdminSID(nullptr),
  m_pAllAppsSID(nullptr),
  m_sd{},
  m_sacl{}
{
  // Create necessary SIDs:
  {
    SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY SIDAuthAppPackage = SECURITY_APP_PACKAGE_AUTHORITY;

    if(!AllocateAndInitializeSid(&SIDAuthNT, 1, SECURITY_INTERACTIVE_RID, 0, 0, 0, 0, 0, 0, 0, &m_pInteractiveSID))
      throw std::runtime_error("Failed to create SID for SECURITY_INTERACTIVE_RID");
    if(!AllocateAndInitializeSid(&SIDAuthNT, 1, SECURITY_AUTHENTICATED_USER_RID, 0, 0, 0, 0, 0, 0, 0, &m_pLocalUsersSID))
      throw std::runtime_error("Failed to create SID for SECURITY_AUTHENTICATED_USER_RID");
    if(!AllocateAndInitializeSid(&SIDAuthNT, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &m_pAdminSID))
      throw std::runtime_error("Failed to create SID for SECURITY_BUILTIN_DOMAIN_RID");
    if(!AllocateAndInitializeSid(&SIDAuthAppPackage, SECURITY_BUILTIN_APP_PACKAGE_RID_COUNT, SECURITY_APP_PACKAGE_BASE_RID, SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE, 0, 0, 0, 0, 0, 0, &m_pAllAppsSID))
      throw std::runtime_error("Failed to create SID for SECURITY_APP_PACKAGE_BASE_RID (ALL_APPLICATION_PACKAGES)");
  }

  // Give interactive users all desired read/write permissions
  {
    auto& eaInteractive = m_ea[0];
    eaInteractive.grfAccessPermissions = userPermissions | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE;
    eaInteractive.grfAccessMode = SET_ACCESS;
    eaInteractive.grfInheritance = NO_INHERITANCE;
    eaInteractive.Trustee.pMultipleTrustee = nullptr;
    eaInteractive.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    eaInteractive.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    eaInteractive.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    eaInteractive.Trustee.ptstrName = (LPTSTR) m_pInteractiveSID;
  }

  // Same story with authenticated users
  {
    auto& eaLocal = m_ea[1];
    eaLocal.grfAccessPermissions = userPermissions | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE;
    eaLocal.grfAccessMode = SET_ACCESS;
    eaLocal.grfInheritance = NO_INHERITANCE;
    eaLocal.Trustee.pMultipleTrustee = nullptr;
    eaLocal.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    eaLocal.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    eaLocal.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    eaLocal.Trustee.ptstrName = (LPTSTR) m_pLocalUsersSID;
  }

  // Administrators can do as they please
  {
    auto& eaAdmins = m_ea[2];
    eaAdmins.grfAccessPermissions = adminPermissions | STANDARD_RIGHTS_ALL;
    eaAdmins.grfAccessMode = SET_ACCESS;
    eaAdmins.grfInheritance = NO_INHERITANCE;
    eaAdmins.Trustee.pMultipleTrustee = nullptr;
    eaAdmins.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    eaAdmins.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    eaAdmins.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    eaAdmins.Trustee.ptstrName = (LPTSTR) m_pAdminSID;
  }

  // Allow read/write persmissions to AppPackages (UWP)
  {
    auto& eaAllApps = m_ea[3];
    eaAllApps.grfAccessPermissions = userPermissions | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE;
    eaAllApps.grfAccessMode = SET_ACCESS;
    eaAllApps.grfInheritance = NO_INHERITANCE;
    eaAllApps.Trustee.pMultipleTrustee = nullptr;
    eaAllApps.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    eaAllApps.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    eaAllApps.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    eaAllApps.Trustee.ptstrName = (LPTSTR) m_pAllAppsSID;
  }

  // Import access list:
  PACL pAcl = nullptr;
  if(SetEntriesInAcl(4, m_ea, nullptr, &pAcl))
    throw std::runtime_error("Failed to set entries in ACL");

  // Security attributes must allow general access by anyone:
  InitializeSecurityDescriptor(&m_sd, SECURITY_DESCRIPTOR_REVISION);
  SetSecurityDescriptorDacl(&m_sd, true, pAcl, true);

  m_sacl.nLength = sizeof(m_sacl);
  m_sacl.lpSecurityDescriptor = &m_sd;
}

DefaultSecurityDescriptor::~DefaultSecurityDescriptor(void) {
  if(m_pInteractiveSID)
    FreeSid(m_pInteractiveSID);
  if(m_pLocalUsersSID)
    FreeSid(m_pLocalUsersSID);
  if(m_pAdminSID)
    FreeSid(m_pAdminSID);
  if (m_pAllAppsSID)
      FreeSid(m_pAllAppsSID);
}

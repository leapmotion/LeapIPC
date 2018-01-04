// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#pragma once
#include <AclAPI.h>

/// <summary>
/// Describes a security descriptor used to secure most Windows service entities
/// </summary>
class DefaultSecurityDescriptor {
public:
  /// <summary>
  /// Creates a security descriptor including administrators and authenticated users
  /// </summary>
  DefaultSecurityDescriptor(DWORD userPermissions, DWORD adminPermissions);
  ~DefaultSecurityDescriptor(void);

private:
  PSID m_pInteractiveSID;
  PSID m_pLocalUsersSID;
  PSID m_pAdminSID;


  EXPLICIT_ACCESS m_ea[3];
  SECURITY_DESCRIPTOR m_sd;
  SECURITY_ATTRIBUTES m_sacl;

public:
  SECURITY_ATTRIBUTES* operator&(void) { return &m_sacl; }
};

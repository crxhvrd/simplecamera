/*
    IGCS Connector Bridge for Simple Camera Plugin

    Implements the IGCS protocol so that ReShade's IGCS Connector
    addon can read our camera state and control screenshot sessions.

    Protocol summary:
    - Our ASI exports 4 functions that IGCS Connector discovers via
   GetProcAddress
    - We discover the connector's buffer functions the same way
    - We write CameraToolsData to the shared buffer every frame
*/

#pragma once

#include "external\scripthook_sdk\inc\main.h"
#include <cmath>
#include <cstdint>

// ============================================================
//  CameraToolsData struct (matches IGCS Connector expectation)
// ============================================================

struct IgcsVec3 {
  float values[3];
  void set(float x, float y, float z) {
    values[0] = x;
    values[1] = y;
    values[2] = z;
  }
};

struct IgcsVec4 {
  float values[4];
  void set(float x, float y, float z, float w) {
    values[0] = x;
    values[1] = y;
    values[2] = z;
    values[3] = w;
  }
};

struct CameraToolsData {
  uint8_t cameraEnabled;
  uint8_t cameraMovementLocked;
  uint8_t reserved1;
  uint8_t reserved2;
  float fov;
  IgcsVec3 coordinates;
  IgcsVec4 lookQuaternion;
  IgcsVec3 rotationMatrixUpVector;
  IgcsVec3 rotationMatrixRightVector;
  IgcsVec3 rotationMatrixForwardVector;
  float pitch;
  float yaw;
  float roll;
};

// ============================================================
//  Public API
// ============================================================

// Try to connect to IGCS Connector addon (call once at startup or periodically)
void IGCS_TryConnect();

// Write current camera state to shared buffer (call every frame)
void IGCS_UpdateData(float posX, float posY, float posZ, float pitch, float yaw,
                     float roll, float fov, bool cameraEnabled);

// Check if connected
bool IGCS_IsConnected();

// Check if a screenshot/DoF session is active (camera locked by IGCS)
bool IGCS_IsSessionActive();

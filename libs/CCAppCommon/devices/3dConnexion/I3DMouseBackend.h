#pragma once
// ##########################################################################
// #                                                                        #
// #                              CLOUDCOMPARE                              #
// #                                                                        #
// #  This program is free software; you can redistribute it and/or modify  #
// #  it under the terms of the GNU General Public License as published by  #
// #  the Free Software Foundation; version 2 or later of the License.      #
// #                                                                        #
// #  This program is distributed in the hope that it will be useful,       #
// #  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
// #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
// #  GNU General Public License for more details.                          #
// #                                                                        #
// #                    COPYRIGHT: CloudCompare project                     #
// #                                                                        #
// ##########################################################################

#include "CCAppCommon.h"

// Qt
#include <QObject>
#include <QString>

// system
#include <vector>

class QWidget;
class ccMainAppInterface;

//! Abstract interface for 3D mouse backends
/** Both the modern navlib backend and legacy 3DxWare backend implement this.
    The navlib backend drives the camera directly via driver accessors, while
    the legacy backend emits motion/button signals for the manager to handle.
**/
class CCAPPCOMMON_LIB_API I3DMouseBackend : public QObject
{
	Q_OBJECT

  public:
	explicit I3DMouseBackend(QObject* parent)
	    : QObject(parent)
	{
	}

	~I3DMouseBackend() override = default;

	//! Attempts to connect to the 3D mouse device/driver
	virtual bool connect(QWidget* mainWidget, QString appName) = 0;

	//! Disconnects from the 3D mouse device/driver
	virtual void disconnect() = 0;

	//! Returns whether the backend is currently connected
	virtual bool isConnected() const = 0;

	//! Returns the backend name for logging
	virtual QString backendName() const = 0;

	//! Whether this backend drives the camera directly (navlib) vs emitting motion signals (legacy)
	virtual bool drivesCameraDirectly() const = 0;

	//! Set the app interface for camera/scene access (used by navlib backend)
	virtual void setAppInterface(ccMainAppInterface* appInterface) { Q_UNUSED(appInterface); }

  Q_SIGNALS:

	//! Emitted when 3D mouse motion data is available (legacy backend only)
	void sigMove3d(std::vector<float>& motionData);
	//! Emitted when the 3D mouse knob is released (legacy backend only)
	void sigReleased();
	//! Emitted when a 3D mouse button is pressed
	void sigOn3dmouseKeyDown(int virtualKeyCode);
	//! Emitted when a 3D mouse command is pressed
	void sigOn3dmouseCMDKeyDown(int virtualCMDCode);
	//! Emitted when a 3D mouse button is released
	void sigOn3dmouseKeyUp(int virtualKeyCode);
	//! Emitted when a 3D mouse command is released
	void sigOn3dmouseCMDKeyUp(int virtualCMDCode);
};

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
// #          COPYRIGHT: CloudCompare project                               #
// #                                                                        #
// ##########################################################################

#include "cc3DMouseManager.h"

#include "I3DMouseBackend.h"
#include "ccGLWindowInterface.h"
#include "ccLog.h"
#include "ccMainAppInterface.h"

#ifdef CC_3DMOUSE_NAVLIB
#include "navlib/Mouse3DNavlib.h"
#endif

#ifdef CC_3DMOUSE_LEGACY
#include "legacy/Mouse3DLegacy.h"
#endif

#include <QAction>
#include <QMainWindow>
#include <QMenu>

cc3DMouseManager::cc3DMouseManager(ccMainAppInterface* appInterface, QObject* parent)
    : QObject(parent)
    , m_appInterface(appInterface)
    , m_backend(nullptr)
{
	setupMenu();

	enableDevice(true, true);
}

cc3DMouseManager::~cc3DMouseManager()
{
	releaseDevice();

	if (m_menu)
	{
		delete m_menu;
	}
}

void cc3DMouseManager::enableDevice(bool state, bool silent)
{
	if (m_backend)
	{
		releaseDevice();
	}

	if (state)
	{
		bool connected = false;

#ifdef CC_3DMOUSE_NAVLIB
		// Try navlib first (modern SDK, respects 3DxWare settings)
		{
			auto* navlibBackend = new Mouse3DNavlib(this);
			navlibBackend->setAppInterface(m_appInterface);
			if (navlibBackend->connect(m_appInterface->getMainWindow(), "CloudCompare"))
			{
				m_backend = navlibBackend;
				connected = true;
				ccLog::Print("[3D Mouse] Using navlib backend");
			}
			else
			{
				delete navlibBackend;
				ccLog::Print("[3D Mouse] navlib backend not available, trying legacy...");
			}
		}
#endif

#ifdef CC_3DMOUSE_LEGACY
		if (!connected)
		{
			// Fall back to legacy SDK
			auto* legacyBackend = new Mouse3DLegacy(this);
			if (legacyBackend->connect(m_appInterface->getMainWindow(), "CloudCompare"))
			{
				m_backend = legacyBackend;
				connected = true;
				ccLog::Print("[3D Mouse] Using legacy 3DxWare SDK backend");

				// Legacy backend emits signals that we handle manually
				QObject::connect(m_backend, &I3DMouseBackend::sigMove3d, this, &cc3DMouseManager::on3DMouseMove);
				QObject::connect(m_backend, &I3DMouseBackend::sigReleased, this, &cc3DMouseManager::on3DMouseReleased);
				QObject::connect(m_backend, &I3DMouseBackend::sigOn3dmouseKeyDown, this, &cc3DMouseManager::on3DMouseKeyDown);
				QObject::connect(m_backend, &I3DMouseBackend::sigOn3dmouseKeyUp, this, &cc3DMouseManager::on3DMouseKeyUp);
				QObject::connect(m_backend, &I3DMouseBackend::sigOn3dmouseCMDKeyDown, this, &cc3DMouseManager::on3DMouseCMDKeyDown);
				QObject::connect(m_backend, &I3DMouseBackend::sigOn3dmouseCMDKeyUp, this, &cc3DMouseManager::on3DMouseCMDKeyUp);
			}
			else
			{
				delete legacyBackend;
			}
		}
#endif

		if (!connected)
		{
			if (!silent)
			{
				ccLog::Error("[3D Mouse] No device found");
			}
			state = false;
		}
	}
	else
	{
		ccLog::Warning("[3D Mouse] Device has been disabled");
	}

	m_actionEnable->blockSignals(true);
	m_actionEnable->setChecked(state);
	m_actionEnable->blockSignals(false);
}

void cc3DMouseManager::releaseDevice()
{
	if (m_backend == nullptr)
		return;

	m_backend->disconnect();
	m_backend->QObject::disconnect(this);

	delete m_backend;
	m_backend = nullptr;
}

void cc3DMouseManager::on3DMouseKeyUp(int)
{
	// nothing right now
}

void cc3DMouseManager::on3DMouseCMDKeyUp(int)
{
	// nothing right now
}

void cc3DMouseManager::on3DMouseKeyDown(int key)
{
#ifdef CC_3DMOUSE_LEGACY
	switch (key)
	{
	case Mouse3DLegacy::V3DK_MENU:
		// should be handled by the driver now!
		break;
	case Mouse3DLegacy::V3DK_FIT:
	{
		if (m_appInterface->getSelectedEntities().empty())
		{
			m_appInterface->setGlobalZoom();
		}
		else
		{
			m_appInterface->zoomOnSelectedEntities();
		}
	}
	break;
	case Mouse3DLegacy::V3DK_TOP:
		m_appInterface->setView(CC_TOP_VIEW);
		break;
	case Mouse3DLegacy::V3DK_LEFT:
		m_appInterface->setView(CC_LEFT_VIEW);
		break;
	case Mouse3DLegacy::V3DK_RIGHT:
		m_appInterface->setView(CC_RIGHT_VIEW);
		break;
	case Mouse3DLegacy::V3DK_FRONT:
		m_appInterface->setView(CC_FRONT_VIEW);
		break;
	case Mouse3DLegacy::V3DK_BOTTOM:
		m_appInterface->setView(CC_BOTTOM_VIEW);
		break;
	case Mouse3DLegacy::V3DK_BACK:
		m_appInterface->setView(CC_BACK_VIEW);
		break;
	case Mouse3DLegacy::V3DK_ROTATE:
		// should be handled by the driver now!
		break;
	case Mouse3DLegacy::V3DK_PANZOOM:
		// should be handled by the driver now!
		break;
	case Mouse3DLegacy::V3DK_ISO1:
		m_appInterface->setView(CC_ISO_VIEW_1);
		break;
	case Mouse3DLegacy::V3DK_ISO2:
		m_appInterface->setView(CC_ISO_VIEW_2);
		break;
	case Mouse3DLegacy::V3DK_PLUS:
		// should be handled by the driver now!
		break;
	case Mouse3DLegacy::V3DK_MINUS:
		// should be handled by the driver now!
		break;
	case Mouse3DLegacy::V3DK_DOMINANT:
		// should be handled by the driver now!
		break;
	case Mouse3DLegacy::V3DK_CW:
	case Mouse3DLegacy::V3DK_CCW:
	{
		ccGLWindowInterface* activeWin = m_appInterface->getActiveGLWindow();
		if (activeWin != nullptr)
		{
			CCVector3d  axis(0, 0, -1);
			CCVector3d  trans(0, 0, 0);
			ccGLMatrixd mat;
			double      angle = M_PI / 2;
			if (key == Mouse3DLegacy::V3DK_CCW)
			{
				angle = -angle;
			}
			mat.initFromParameters(angle, axis, trans);
			activeWin->rotateBaseViewMat(mat);
			activeWin->redraw();
		}
	}
	break;
	case Mouse3DLegacy::V3DK_ESC:
	case Mouse3DLegacy::V3DK_ALT:
	case Mouse3DLegacy::V3DK_SHIFT:
	case Mouse3DLegacy::V3DK_CTRL:
	default:
		ccLog::Warning("[3D mouse] This button is not handled (yet)");
		// TODO
		break;
	}
#else
	Q_UNUSED(key);
#endif
}

void cc3DMouseManager::on3DMouseCMDKeyDown(int cmd)
{
#ifdef CC_3DMOUSE_LEGACY
	switch (cmd)
	{
	case Mouse3DLegacy::V3DCMD_VIEW_FIT:
	{
		if (m_appInterface->getSelectedEntities().empty())
		{
			m_appInterface->setGlobalZoom();
		}
		else
		{
			m_appInterface->zoomOnSelectedEntities();
		}
	}
	break;
	case Mouse3DLegacy::V3DCMD_VIEW_TOP:
		m_appInterface->setView(CC_TOP_VIEW);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_LEFT:
		m_appInterface->setView(CC_LEFT_VIEW);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_RIGHT:
		m_appInterface->setView(CC_RIGHT_VIEW);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_FRONT:
		m_appInterface->setView(CC_FRONT_VIEW);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_BOTTOM:
		m_appInterface->setView(CC_BOTTOM_VIEW);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_BACK:
		m_appInterface->setView(CC_BACK_VIEW);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_ISO1:
		m_appInterface->setView(CC_ISO_VIEW_1);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_ISO2:
		m_appInterface->setView(CC_ISO_VIEW_2);
		break;
	case Mouse3DLegacy::V3DCMD_VIEW_ROLLCW:
	case Mouse3DLegacy::V3DCMD_VIEW_ROLLCCW:
	{
		ccGLWindowInterface* activeWin = m_appInterface->getActiveGLWindow();
		if (activeWin != nullptr)
		{
			CCVector3d  axis(0, 0, -1);
			CCVector3d  trans(0, 0, 0);
			ccGLMatrixd mat;
			double      angle = M_PI / 2;
			if (cmd == Mouse3DLegacy::V3DCMD_VIEW_ROLLCCW)
			{
				angle = -angle;
			}
			mat.initFromParameters(angle, axis, trans);
			activeWin->rotateBaseViewMat(mat);
			activeWin->redraw();
		}
	}
	break;
	case Mouse3DLegacy::V3DCMD_VIEW_SPINCW:
	case Mouse3DLegacy::V3DCMD_VIEW_SPINCCW:
	{
		ccGLWindowInterface* activeWin = m_appInterface->getActiveGLWindow();
		if (activeWin != nullptr)
		{
			CCVector3d  axis(0, 1, 0);
			CCVector3d  trans(0, 0, 0);
			ccGLMatrixd mat;
			double      angle = M_PI / 2;
			if (cmd == Mouse3DLegacy::V3DCMD_VIEW_SPINCCW)
			{
				angle = -angle;
			}
			mat.initFromParameters(angle, axis, trans);
			activeWin->rotateBaseViewMat(mat);
			activeWin->redraw();
		}
	}
	break; // Note: original code was missing this break (fall-through bug)
	case Mouse3DLegacy::V3DCMD_VIEW_TILTCW:
	case Mouse3DLegacy::V3DCMD_VIEW_TILTCCW:
	{
		ccGLWindowInterface* activeWin = m_appInterface->getActiveGLWindow();
		if (activeWin != nullptr)
		{
			CCVector3d  axis(1, 0, 0);
			CCVector3d  trans(0, 0, 0);
			ccGLMatrixd mat;
			double      angle = M_PI / 2;
			if (cmd == Mouse3DLegacy::V3DCMD_VIEW_TILTCCW)
			{
				angle = -angle;
			}
			mat.initFromParameters(angle, axis, trans);
			activeWin->rotateBaseViewMat(mat);
			activeWin->redraw();
		}
	}
	break;
	default:
		ccLog::Warning("[3D mouse] This button is not handled (yet)");
		// TODO
		break;
	}
#else
	Q_UNUSED(cmd);
#endif
}

void cc3DMouseManager::on3DMouseMove(std::vector<float>& vec)
{
#ifdef CC_3DMOUSE_LEGACY
	ccGLWindowInterface* win = m_appInterface->getActiveGLWindow();
	if (win == nullptr)
		return;

	Mouse3DLegacy::Apply(vec, win);
#else
	Q_UNUSED(vec);
#endif
}

void cc3DMouseManager::on3DMouseReleased()
{
	ccGLWindowInterface* win = m_appInterface->getActiveGLWindow();
	if (win == nullptr)
		return;

	if (win->getPivotVisibility() == ccGLWindowInterface::PIVOT_SHOW_ON_MOVE)
	{
		// we have to hide the pivot symbol!
		win->showPivotSymbol(false);
		win->redraw();
	}
}

void cc3DMouseManager::setupMenu()
{
	m_menu = new QMenu(tr("3D mouse"));
	m_menu->setIcon(QIcon(":/CC/images/im3DxLogo.png"));

	m_actionEnable = new QAction(tr("Enable"), this);
	m_actionEnable->setCheckable(true);

	QObject::connect(m_actionEnable, &QAction::toggled, [this](bool state)
	        { enableDevice(state, false); });

	m_menu->addAction(m_actionEnable);
}

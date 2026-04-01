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

#include "Mouse3DNavlib.h"

// CC
#include <ccBBox.h>
#include <ccGLWindowInterface.h>
#include <ccHObject.h>
#include <ccLog.h>
#include <ccMainAppInterface.h>
#include <ccViewportParameters.h>

// 3Dconnexion navlib
#include <SpaceMouse/CCategory.hpp>
#include <SpaceMouse/CCommand.hpp>
#include <SpaceMouse/CCommandSet.hpp>

// Qt
#include <QMetaObject>

// system
#include <cmath>

// CC view command IDs for navlib button mapping
static const char* CC_CMD_FIT         = "CC_FIT";
static const char* CC_CMD_VIEW_TOP    = "CC_VIEW_TOP";
static const char* CC_CMD_VIEW_BOTTOM = "CC_VIEW_BOTTOM";
static const char* CC_CMD_VIEW_FRONT  = "CC_VIEW_FRONT";
static const char* CC_CMD_VIEW_BACK   = "CC_VIEW_BACK";
static const char* CC_CMD_VIEW_LEFT   = "CC_VIEW_LEFT";
static const char* CC_CMD_VIEW_RIGHT  = "CC_VIEW_RIGHT";
static const char* CC_CMD_VIEW_ISO1   = "CC_VIEW_ISO1";
static const char* CC_CMD_VIEW_ISO2   = "CC_VIEW_ISO2";

Mouse3DNavlib::Mouse3DNavlib(QObject* parent)
    : I3DMouseBackend(parent)
    // Single-threaded (callbacks come via the app's message loop), row vectors (default)
    , CNavigation3D(false, false)
{
}

Mouse3DNavlib::~Mouse3DNavlib()
{
	disconnect();
}

void Mouse3DNavlib::setAppInterface(ccMainAppInterface* appInterface)
{
	m_appInterface = appInterface;
}

bool Mouse3DNavlib::connect(QWidget* /*mainWidget*/, QString appName)
{
	if (!m_appInterface)
	{
		ccLog::Warning("[3D Mouse] navlib backend: no app interface set");
		return false;
	}

	try
	{
		PutProfileHint(appName.toStdString());
		EnableNavigation(true);
		PutFrameTimingSource(TimingSource::SpaceMouse);

		exportCommands();

		ccLog::Print(tr("[3D Mouse] navlib backend connected (profile: %1)").arg(appName));
		return true;
	}
	catch (const std::system_error& e)
	{
		ccLog::Warning(tr("[3D Mouse] navlib connection failed: %1 (code %2)").arg(e.what()).arg(e.code().value()));
		return false;
	}
	catch (const std::exception& e)
	{
		ccLog::Warning(tr("[3D Mouse] navlib connection failed: %1").arg(e.what()));
		return false;
	}
}

void Mouse3DNavlib::disconnect()
{
	if (IsEnabled())
	{
		try
		{
			EnableNavigation(false);
		}
		catch (...)
		{
		}
	}
}

bool Mouse3DNavlib::isConnected() const
{
	return IsEnabled();
}

ccGLWindowInterface* Mouse3DNavlib::getActiveWindow() const
{
	if (m_appInterface)
	{
		return m_appInterface->getActiveGLWindow();
	}
	return nullptr;
}

void Mouse3DNavlib::exportCommands()
{
	using TDx::SpaceMouse::CCategory;
	using TDx::SpaceMouse::CCommand;
	using TDx::SpaceMouse::CCommandSet;

	CCommandSet commandSet("CloudCompare", "CloudCompare");

	CCategory viewCategory("CC_Views", "Views");
	viewCategory.push_back(CCommand(CC_CMD_FIT, "Fit All / Fit Selection"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_TOP, "Top View"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_BOTTOM, "Bottom View"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_FRONT, "Front View"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_BACK, "Back View"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_LEFT, "Left View"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_RIGHT, "Right View"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_ISO1, "Isometric View 1"));
	viewCategory.push_back(CCommand(CC_CMD_VIEW_ISO2, "Isometric View 2"));
	commandSet.push_back(std::move(viewCategory));

	try
	{
		nav3d::AddCommandSet(commandSet);
		PutActiveCommands("CloudCompare");
	}
	catch (const std::exception& e)
	{
		ccLog::Warning(tr("[3D Mouse] Failed to export commands to navlib: %1").arg(e.what()));
	}
}

// ============================================================================
// ISpace3D accessors
// ============================================================================

long Mouse3DNavlib::GetCoordinateSystem(navlib::matrix_t& matrix) const
{
	// CC uses a right-handed coordinate system: X-right, Y-up, Z-out (OpenGL convention)
	// This matches the navlib default
	navlib::matrix_t cs = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	matrix = cs;
	return 0;
}

long Mouse3DNavlib::GetFrontView(navlib::matrix_t& matrix) const
{
	// Front view: camera looks along -Z (OpenGL default = identity camera-to-world)
	navlib::matrix_t front = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	matrix = front;
	return 0;
}

// ============================================================================
// IView accessors
// ============================================================================

long Mouse3DNavlib::GetCameraMatrix(navlib::matrix_t& matrix) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	const ccViewportParameters& params  = win->getViewportParameters();
	const ccGLMatrixd&          viewMat = params.viewMat;
	const double*               vm      = viewMat.data();
	const CCVector3d&           camPos  = params.getCameraCenter();

	// CC's viewMat is a world-to-camera rotation stored column-major.
	// The navlib camera matrix is a camera-to-world affine, row-major, row vectors.
	// Camera-to-world rotation = viewMat^T.
	//
	// navlib row i = camera axis i in world = viewMat row i
	// viewMat(i,j) = vm[j*4 + i]  (column-major storage)
	// So navlib(row, col) = viewMat(row, col) = vm[col*4 + row]
	matrix.m00 = vm[0];  matrix.m01 = vm[4];  matrix.m02 = vm[8];   matrix.m03 = 0;
	matrix.m10 = vm[1];  matrix.m11 = vm[5];  matrix.m12 = vm[9];   matrix.m13 = 0;
	matrix.m20 = vm[2];  matrix.m21 = vm[6];  matrix.m22 = vm[10];  matrix.m23 = 0;
	matrix.m30 = camPos.x; matrix.m31 = camPos.y; matrix.m32 = camPos.z; matrix.m33 = 1;

	return 0;
}

long Mouse3DNavlib::GetCameraTarget(navlib::point_t& target) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	const ccViewportParameters& params = win->getViewportParameters();

	if (params.objectCenteredView)
	{
		const CCVector3d& pivot = params.getPivotPoint();
		target.x = pivot.x;
		target.y = pivot.y;
		target.z = pivot.z;
		return 0;
	}

	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Mouse3DNavlib::GetPointerPosition(navlib::point_t& position) const
{
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Mouse3DNavlib::GetViewExtents(navlib::box_t& extents) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	const ccViewportParameters& params = win->getViewportParameters();

	double width   = params.computeWidthAtFocalDist(win->glWidth(), win->glHeight());
	double halfW   = width / 2.0;
	double aspect  = static_cast<double>(win->glHeight()) / static_cast<double>(win->glWidth());
	double halfH   = halfW * aspect;

	extents.min.x = -halfW;
	extents.min.y = -halfH;
	extents.min.z = 0;
	extents.max.x = halfW;
	extents.max.y = halfH;
	extents.max.z = 0;

	return 0;
}

long Mouse3DNavlib::GetViewFOV(double& fov) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	const ccViewportParameters& params = win->getViewportParameters();
	// navlib expects FOV in radians
	fov = static_cast<double>(params.fov_deg) * M_PI / 180.0;
	return 0;
}

long Mouse3DNavlib::GetViewFrustum(navlib::frustum_t& frustum) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	const ccViewportParameters& params = win->getViewportParameters();

	double fovRad  = static_cast<double>(params.fov_deg) * M_PI / 180.0;
	double nearVal = params.zNear;
	double farVal  = params.zFar;
	double ar      = static_cast<double>(win->glWidth()) / static_cast<double>(win->glHeight());

	double top   = nearVal * tan(fovRad / 2.0);
	double right = top * ar;

	frustum.left   = -right;
	frustum.right  = right;
	frustum.bottom = -top;
	frustum.top    = top;
	frustum.nearVal = nearVal;
	frustum.farVal  = farVal;

	return 0;
}

long Mouse3DNavlib::GetIsViewPerspective(navlib::bool_t& perspective) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	perspective = win->getViewportParameters().perspectiveView ? 1 : 0;
	return 0;
}

long Mouse3DNavlib::GetIsViewRotatable(navlib::bool_t& isRotatable) const
{
	isRotatable = 1;
	return 0;
}

long Mouse3DNavlib::GetViewFocusDistance(double& distance) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	distance = win->getViewportParameters().getFocalDistance();
	return 0;
}

long Mouse3DNavlib::SetCameraMatrix(const navlib::matrix_t& matrix)
{
	ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	ccViewportParameters params = win->getViewportParameters();

	// Convert navlib camera-to-world (row-major, row vectors) to CC's viewMat
	// (world-to-camera rotation, column-major).
	// viewMat = transpose of navlib's 3x3 rotation part.
	// CC column-major: data[j*4 + i] = navlib(i, j)
	ccGLMatrixd newViewMat;
	double*     vm = newViewMat.data();
	vm[0]  = matrix.m00; vm[1]  = matrix.m10; vm[2]  = matrix.m20; vm[3]  = 0;
	vm[4]  = matrix.m01; vm[5]  = matrix.m11; vm[6]  = matrix.m21; vm[7]  = 0;
	vm[8]  = matrix.m02; vm[9]  = matrix.m12; vm[10] = matrix.m22; vm[11] = 0;
	vm[12] = 0;          vm[13] = 0;          vm[14] = 0;          vm[15] = 1;

	params.viewMat = newViewMat;

	CCVector3d newCamCenter(matrix.m30, matrix.m31, matrix.m32);
	// Set camera center and auto-update focal distance from pivot-camera distance
	params.setCameraCenter(newCamCenter, true);

	win->setViewportParameters(params);
	return 0;
}

long Mouse3DNavlib::SetCameraTarget(const navlib::point_t& target)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long Mouse3DNavlib::SetPointerPosition(const navlib::point_t& position)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long Mouse3DNavlib::SetViewExtents(const navlib::box_t& extents)
{
	ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	ccViewportParameters params = win->getViewportParameters();

	double newWidth     = extents.max.x - extents.min.x;
	double currentWidth = params.computeWidthAtFocalDist(win->glWidth(), win->glHeight());

	if (currentWidth > 0 && newWidth > 0)
	{
		double ratio = newWidth / currentWidth;
		params.setFocalDistance(params.getFocalDistance() * ratio);
		win->setViewportParameters(params);
	}

	return 0;
}

long Mouse3DNavlib::SetViewFOV(double fov)
{
	ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	ccViewportParameters params = win->getViewportParameters();
	params.fov_deg = static_cast<float>(fov * 180.0 / M_PI);
	win->setViewportParameters(params);
	return 0;
}

long Mouse3DNavlib::SetViewFrustum(const navlib::frustum_t& frustum)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

// ============================================================================
// IModel accessors
// ============================================================================

long Mouse3DNavlib::GetModelExtents(navlib::box_t& extents) const
{
	if (!m_appInterface)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	ccHObject* root = m_appInterface->dbRootObject();
	if (!root)
	{
		return navlib::make_result_code(navlib::navlib_errc::no_data_available);
	}

	ccBBox bbox = root->getBB_recursive();
	if (!bbox.isValid())
	{
		return navlib::make_result_code(navlib::navlib_errc::no_data_available);
	}

	const CCVector3& bbMin = bbox.minCorner();
	const CCVector3& bbMax = bbox.maxCorner();

	extents.min.x = bbMin.x;
	extents.min.y = bbMin.y;
	extents.min.z = bbMin.z;
	extents.max.x = bbMax.x;
	extents.max.y = bbMax.y;
	extents.max.z = bbMax.z;

	return 0;
}

long Mouse3DNavlib::GetSelectionExtents(navlib::box_t& extents) const
{
	if (!m_appInterface)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	const ccHObject::Container& selection = m_appInterface->getSelectedEntities();
	if (selection.empty())
	{
		return navlib::make_result_code(navlib::navlib_errc::no_data_available);
	}

	ccBBox totalBBox;
	for (ccHObject* entity : selection)
	{
		totalBBox += entity->getBB_recursive();
	}

	if (!totalBBox.isValid())
	{
		return navlib::make_result_code(navlib::navlib_errc::no_data_available);
	}

	const CCVector3& bbMin = totalBBox.minCorner();
	const CCVector3& bbMax = totalBBox.maxCorner();

	extents.min.x = bbMin.x;
	extents.min.y = bbMin.y;
	extents.min.z = bbMin.z;
	extents.max.x = bbMax.x;
	extents.max.y = bbMax.y;
	extents.max.z = bbMax.z;

	return 0;
}

long Mouse3DNavlib::GetSelectionTransform(navlib::matrix_t& transform) const
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long Mouse3DNavlib::GetIsSelectionEmpty(navlib::bool_t& empty) const
{
	if (!m_appInterface)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	empty = m_appInterface->getSelectedEntities().empty() ? 1 : 0;
	return 0;
}

long Mouse3DNavlib::SetSelectionTransform(const navlib::matrix_t& matrix)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long Mouse3DNavlib::GetUnitsToMeters(double& meters) const
{
	// CC doesn't define a physical unit - assume 1 unit = 1 meter
	meters = 1.0;
	return 0;
}

long Mouse3DNavlib::GetFloorPlane(navlib::plane_t& floor) const
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

// ============================================================================
// IPivot accessors
// ============================================================================

long Mouse3DNavlib::GetPivotPosition(navlib::point_t& position) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	const ccViewportParameters& params = win->getViewportParameters();
	const CCVector3d& pivot = params.getPivotPoint();

	position.x = pivot.x;
	position.y = pivot.y;
	position.z = pivot.z;

	return 0;
}

long Mouse3DNavlib::GetPivotVisible(navlib::bool_t& visible) const
{
	const ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	visible = (win->getPivotVisibility() != ccGLWindowInterface::PIVOT_HIDE) ? 1 : 0;
	return 0;
}

long Mouse3DNavlib::IsUserPivot(navlib::bool_t& userPivot) const
{
	// CC doesn't track user-set pivots explicitly
	userPivot = 0;
	return 0;
}

long Mouse3DNavlib::SetPivotPosition(const navlib::point_t& position)
{
	ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	ccViewportParameters params = win->getViewportParameters();
	params.setPivotPoint(CCVector3d(position.x, position.y, position.z), true);
	win->setViewportParameters(params);

	return 0;
}

long Mouse3DNavlib::SetPivotVisible(bool visible)
{
	ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return navlib::make_result_code(navlib::navlib_errc::invalid_operation);
	}

	win->showPivotSymbol(visible);
	return 0;
}

// ============================================================================
// IHit accessors
// ============================================================================

long Mouse3DNavlib::GetHitLookAt(navlib::point_t& position) const
{
	// Hit testing not implemented - navlib will use model extents for auto-pivot
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Mouse3DNavlib::SetHitAperture(double aperture)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long Mouse3DNavlib::SetHitDirection(const navlib::vector_t& direction)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long Mouse3DNavlib::SetHitLookFrom(const navlib::point_t& eye)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long Mouse3DNavlib::SetHitSelectionOnly(bool onlySelection)
{
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

// ============================================================================
// IState accessors
// ============================================================================

long Mouse3DNavlib::SetTransaction(long transaction)
{
	if (transaction == 0)
	{
		// End of navigation frame - trigger redraw
		ccGLWindowInterface* win = getActiveWindow();
		if (win)
		{
			win->redraw();
		}
	}
	return 0;
}

long Mouse3DNavlib::SetMotionFlag(bool motion)
{
	ccGLWindowInterface* win = getActiveWindow();
	if (!win)
	{
		return 0;
	}

	if (motion)
	{
		win->showPivotSymbol(true);
	}
	else
	{
		if (win->getPivotVisibility() == ccGLWindowInterface::PIVOT_SHOW_ON_MOVE)
		{
			win->showPivotSymbol(false);
			win->redraw();
		}
	}
	return 0;
}

// ============================================================================
// IEvents accessors
// ============================================================================

long Mouse3DNavlib::SetActiveCommand(std::string commandId)
{
	if (!m_appInterface || commandId.empty())
	{
		return 0;
	}

	if (commandId == CC_CMD_FIT)
	{
		if (m_appInterface->getSelectedEntities().empty())
			m_appInterface->setGlobalZoom();
		else
			m_appInterface->zoomOnSelectedEntities();
	}
	else if (commandId == CC_CMD_VIEW_TOP)
	{
		m_appInterface->setView(CC_TOP_VIEW);
	}
	else if (commandId == CC_CMD_VIEW_BOTTOM)
	{
		m_appInterface->setView(CC_BOTTOM_VIEW);
	}
	else if (commandId == CC_CMD_VIEW_FRONT)
	{
		m_appInterface->setView(CC_FRONT_VIEW);
	}
	else if (commandId == CC_CMD_VIEW_BACK)
	{
		m_appInterface->setView(CC_BACK_VIEW);
	}
	else if (commandId == CC_CMD_VIEW_LEFT)
	{
		m_appInterface->setView(CC_LEFT_VIEW);
	}
	else if (commandId == CC_CMD_VIEW_RIGHT)
	{
		m_appInterface->setView(CC_RIGHT_VIEW);
	}
	else if (commandId == CC_CMD_VIEW_ISO1)
	{
		m_appInterface->setView(CC_ISO_VIEW_1);
	}
	else if (commandId == CC_CMD_VIEW_ISO2)
	{
		m_appInterface->setView(CC_ISO_VIEW_2);
	}
	else
	{
		ccLog::Warning(tr("[3D Mouse] Unknown navlib command: %1").arg(QString::fromStdString(commandId)));
	}

	return 0;
}

long Mouse3DNavlib::SetSettingsChanged(long count)
{
	return 0;
}

long Mouse3DNavlib::SetKeyPress(long vkey)
{
	return 0;
}

long Mouse3DNavlib::SetKeyRelease(long vkey)
{
	return 0;
}

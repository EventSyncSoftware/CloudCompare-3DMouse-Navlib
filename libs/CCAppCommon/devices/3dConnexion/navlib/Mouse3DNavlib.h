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

#include "I3DMouseBackend.h"

// 3Dconnexion navlib
#include <SpaceMouse/CNavigation3D.hpp>

class ccGLWindowInterface;
class ccMainAppInterface;

//! Modern navlib SDK backend for 3D mouse handling (Windows + macOS)
/** Uses the 3Dconnexion Navigation Library (navlib) accessor pattern.
    The navlib driver reads/writes camera and scene state through property
    accessors, respecting all user-configured 3DxWare settings (speed,
    sensitivity, axis inversion, button mappings).
**/
class CCAPPCOMMON_LIB_API Mouse3DNavlib : public I3DMouseBackend,
                                          protected TDx::SpaceMouse::Navigation3D::CNavigation3D
{
	Q_OBJECT

	typedef TDx::SpaceMouse::Navigation3D::CNavigation3D nav3d;

  public:
	explicit Mouse3DNavlib(QObject* parent);
	~Mouse3DNavlib() override;

	// I3DMouseBackend interface
	bool connect(QWidget* mainWidget, QString appName) override;
	void disconnect() override;
	bool isConnected() const override;
	QString backendName() const override { return tr("3Dconnexion navlib"); }
	bool drivesCameraDirectly() const override { return true; }
	void setAppInterface(ccMainAppInterface* appInterface) override;

  protected:
	// --- ISpace3D accessors ---
	long GetCoordinateSystem(navlib::matrix_t& matrix) const override;
	long GetFrontView(navlib::matrix_t& matrix) const override;

	// --- IView accessors ---
	long GetCameraMatrix(navlib::matrix_t& matrix) const override;
	long GetCameraTarget(navlib::point_t& target) const override;
	long GetPointerPosition(navlib::point_t& position) const override;
	long GetViewExtents(navlib::box_t& extents) const override;
	long GetViewFOV(double& fov) const override;
	long GetViewFrustum(navlib::frustum_t& frustum) const override;
	long GetIsViewPerspective(navlib::bool_t& perspective) const override;
	long GetIsViewRotatable(navlib::bool_t& isRotatable) const override;
	long GetViewFocusDistance(double& distance) const override;

	long SetCameraMatrix(const navlib::matrix_t& matrix) override;
	long SetCameraTarget(const navlib::point_t& target) override;
	long SetPointerPosition(const navlib::point_t& position) override;
	long SetViewExtents(const navlib::box_t& extents) override;
	long SetViewFOV(double fov) override;
	long SetViewFrustum(const navlib::frustum_t& frustum) override;

	// --- IModel accessors ---
	long GetModelExtents(navlib::box_t& extents) const override;
	long GetSelectionExtents(navlib::box_t& extents) const override;
	long GetSelectionTransform(navlib::matrix_t& transform) const override;
	long GetIsSelectionEmpty(navlib::bool_t& empty) const override;
	long SetSelectionTransform(const navlib::matrix_t& matrix) override;
	long GetUnitsToMeters(double& meters) const override;
	long GetFloorPlane(navlib::plane_t& floor) const override;

	// --- IPivot accessors ---
	long GetPivotPosition(navlib::point_t& position) const override;
	long GetPivotVisible(navlib::bool_t& visible) const override;
	long IsUserPivot(navlib::bool_t& userPivot) const override;
	long SetPivotPosition(const navlib::point_t& position) override;
	long SetPivotVisible(bool visible) override;

	// --- IHit accessors ---
	long GetHitLookAt(navlib::point_t& position) const override;
	long SetHitAperture(double aperture) override;
	long SetHitDirection(const navlib::vector_t& direction) override;
	long SetHitLookFrom(const navlib::point_t& eye) override;
	long SetHitSelectionOnly(bool onlySelection) override;

	// --- IState accessors ---
	long SetTransaction(long transaction) override;
	long SetMotionFlag(bool motion) override;

	// --- IEvents accessors ---
	long SetActiveCommand(std::string commandId) override;
	long SetSettingsChanged(long count) override;
	long SetKeyPress(long vkey) override;
	long SetKeyRelease(long vkey) override;

  private:
	//! Returns the active GL window, or nullptr
	ccGLWindowInterface* getActiveWindow() const;

	//! Exports CC view commands to the 3DxWare button mapping UI
	void exportCommands();

	ccMainAppInterface* m_appInterface = nullptr;
};

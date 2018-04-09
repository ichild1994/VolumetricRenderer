#pragma once


#include "Common.hpp"
#include "HelperWidgets.hpp"

class ControlPanel: public QDockWidget
{
	Q_OBJECT
	public:
		QScrollArea* scrollAreaBase;
		QWidget* widgetBase; 
		QVBoxLayout* layoutBase;
		
		QCheckBox* checkAxisVisible;
		Vector3Chooser* chooserPosition;		
		Vector3Chooser* chooserRotation;
		Vector3Chooser* chooserScale;
	
		ControlPanel();
};
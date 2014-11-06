#include "ViewerService.h"
#include "binary.h"

// Base Class for service provider
namespace Sgv
{
	void ViewerService::recvMsg(sigverse::RecvMsgEvent &evt)
	{
		m_MsgEvt = evt;
		m_isMsg = true;
	}

	void ViewerService::recvCaptureView(sigverse::RecvCptEvent &evt)
	{
		// Set request type: captureView
		m_requestType = VIEWER_REQUEST_TYPE_CAPTUREVIEW;

		m_CptEvt.setCameraID(evt.getCameraID());
		m_CptEvt.setSender(evt.getSender());
	}

	void ViewerService::recvDistanceSensor(sigverse::RecvDstEvent &evt)
	{
		// Set request type: distanceSensor
		m_requestType = VIEWER_REQUEST_TYPE_DISTANCE_SENSOR;

		// Store as member variable
		m_DstEvt.setType    (evt.getType());
		m_DstEvt.setMax     (evt.getMax());
		m_DstEvt.setMin     (evt.getMin());
		m_DstEvt.setCameraID(evt.getCameraID());
		m_DstEvt.setSender  (evt.getSender());
		//char tmp[64];
		//sprintf(tmp, "id = %d, sender = %s", evt.getCameraID(), evt.getSender().c_str());
		//MessageBox( NULL, tmp, "Error", MB_OK);
	}

	void ViewerService::recvDetectEntities(sigverse::RecvDtcEvent &evt)
	{
		// Set request type: detectEntities
		m_requestType = VIEWER_REQUEST_TYPE_DETECT_ENTITIES;

		m_DtcEvt.setCameraID(evt.getCameraID());
		m_DtcEvt.setSender(evt.getSender());

	}

}


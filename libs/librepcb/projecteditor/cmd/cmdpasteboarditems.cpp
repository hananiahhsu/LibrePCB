/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "cmdpasteboarditems.h"

#include "../boardeditor/boardclipboarddata.h"

#include <librepcb/common/scopeguard.h>
#include <librepcb/project/boards/boardlayerstack.h>
#include <librepcb/project/boards/cmd/cmdboardholeadd.h>
#include <librepcb/project/boards/cmd/cmdboardnetsegmentadd.h>
#include <librepcb/project/boards/cmd/cmdboardnetsegmentaddelements.h>
#include <librepcb/project/boards/cmd/cmdboardplaneadd.h>
#include <librepcb/project/boards/cmd/cmdboardpolygonadd.h>
#include <librepcb/project/boards/cmd/cmdboardstroketextadd.h>
#include <librepcb/project/boards/items/bi_hole.h>
#include <librepcb/project/boards/items/bi_netline.h>
#include <librepcb/project/boards/items/bi_netpoint.h>
#include <librepcb/project/boards/items/bi_netsegment.h>
#include <librepcb/project/boards/items/bi_plane.h>
#include <librepcb/project/boards/items/bi_polygon.h>
#include <librepcb/project/boards/items/bi_stroketext.h>
#include <librepcb/project/boards/items/bi_via.h>
#include <librepcb/project/circuit/circuit.h>
#include <librepcb/project/circuit/cmd/cmdnetclassadd.h>
#include <librepcb/project/circuit/cmd/cmdnetsignaladd.h>
#include <librepcb/project/circuit/netsignal.h>
#include <librepcb/project/project.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPasteBoardItems::CmdPasteBoardItems(Board& board,
                                       std::unique_ptr<BoardClipboardData> data,
                                       const Point& posOffset) noexcept
  : UndoCommandGroup(tr("Paste Board Elements")),
    mProject(board.getProject()),
    mBoard(board),
    mData(std::move(data)),
    mPosOffset(posOffset) {
  Q_ASSERT(mData);
}

CmdPasteBoardItems::~CmdPasteBoardItems() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPasteBoardItems::performExecute() {
  // if an error occurs, undo all already executed child commands
  auto undoScopeGuard = scopeGuard([&]() { performUndo(); });

  // Notes:
  //
  //  - The graphics items of the added elements are selected immediately to
  //    allow dragging them afterwards.

  // Paste net segments
  for (const BoardClipboardData::NetSegment& seg : mData->getNetSegments()) {
    // Add new segment
    BI_NetSegment* copy =
        new BI_NetSegment(mBoard, *getOrCreateNetSignal(*seg.netName));
    copy->setSelected(true);
    execNewChildCmd(new CmdBoardNetSegmentAdd(*copy));

    // Add vias, netpoints and netlines
    QScopedPointer<CmdBoardNetSegmentAddElements> cmdAddElements(
        new CmdBoardNetSegmentAddElements(*copy));
    QHash<Uuid, BI_Via*> viaMap;
    for (const BoardClipboardData::Via& v : seg.vias) {
      BI_Via* via = cmdAddElements->addVia(v.position + mPosOffset, v.shape,
                                           v.size, v.drillDiameter);
      via->setSelected(true);
      viaMap.insert(v.uuid, via);
    }
    QHash<Uuid, BI_NetPoint*> netPointMap;
    for (const BoardClipboardData::NetPoint& np : seg.points) {
      BI_NetPoint* netpoint =
          cmdAddElements->addNetPoint(np.position + mPosOffset);
      netpoint->setSelected(true);
      netPointMap.insert(np.uuid, netpoint);
    }
    for (const BoardClipboardData::NetLine& nl : seg.lines) {
      BI_NetLineAnchor* start = nullptr;
      if (nl.startJunction) {
        start = netPointMap[*nl.startJunction];
        Q_ASSERT(start);
      } else {
        start = viaMap[*nl.startVia];
        Q_ASSERT(start);
      }
      BI_NetLineAnchor* end = nullptr;
      if (nl.endJunction) {
        end = netPointMap[*nl.endJunction];
        Q_ASSERT(end);
      } else {
        end = viaMap[*nl.endVia];
        Q_ASSERT(end);
      }
      GraphicsLayer* layer = mBoard.getLayerStack().getLayer(nl.layer);
      if (!layer) throw LogicError(__FILE__, __LINE__);
      BI_NetLine* netline =
          cmdAddElements->addNetLine(*start, *end, *layer, nl.width);
      netline->setSelected(true);
    }
    execNewChildCmd(cmdAddElements.take());
  }

  // Paste planes
  for (const BoardClipboardData::Plane& plane : mData->getPlanes()) {
    BI_Plane* copy = new BI_Plane(mBoard, Uuid::createRandom(),
                                  GraphicsLayerName(plane.layer),
                                  *getOrCreateNetSignal(plane.netSignalName),
                                  plane.outline.translated(mPosOffset));
    copy->setMinWidth(plane.minWidth);
    copy->setMinClearance(plane.minClearance);
    copy->setKeepOrphans(plane.keepOrphans);
    copy->setPriority(plane.priority);
    copy->setConnectStyle(plane.connectStyle);
    copy->setSelected(true);
    execNewChildCmd(new CmdBoardPlaneAdd(*copy));
  }

  // Paste polygons
  for (const Polygon& polygon : mData->getPolygons()) {
    Polygon copy(Uuid::createRandom(), polygon);          // assign new UUID
    copy.setPath(copy.getPath().translated(mPosOffset));  // move
    BI_Polygon* item = new BI_Polygon(mBoard, copy);
    item->setSelected(true);
    execNewChildCmd(new CmdBoardPolygonAdd(*item));
  }

  // Paste stroke texts
  for (const StrokeText& text : mData->getStrokeTexts()) {
    StrokeText copy(Uuid::createRandom(), text);        // assign new UUID
    copy.setPosition(copy.getPosition() + mPosOffset);  // move
    BI_StrokeText* item = new BI_StrokeText(mBoard, copy);
    item->setSelected(true);
    execNewChildCmd(new CmdBoardStrokeTextAdd(*item));
  }

  // Paste holes
  for (const Hole& hole : mData->getHoles()) {
    Hole copy(Uuid::createRandom(), hole);              // assign new UUID
    copy.setPosition(copy.getPosition() + mPosOffset);  // move
    BI_Hole* item = new BI_Hole(mBoard, copy);
    item->setSelected(true);
    execNewChildCmd(new CmdBoardHoleAdd(*item));
  }

  undoScopeGuard.dismiss();  // no undo required
  return getChildCount() > 0;
}

NetSignal* CmdPasteBoardItems::getOrCreateNetSignal(const QString& name) {
  NetSignal* netSignal = mProject.getCircuit().getNetSignalByName(name);
  if (netSignal) {
    return netSignal;
  }

  // Get or create netclass with the name "default"
  NetClass* netclass =
      mProject.getCircuit().getNetClassByName(ElementName("default"));
  if (!netclass) {
    CmdNetClassAdd* cmd =
        new CmdNetClassAdd(mProject.getCircuit(), ElementName("default"));
    execNewChildCmd(cmd);
    netclass = cmd->getNetClass();
    Q_ASSERT(netclass);
  }

  // Create new net signal
  CmdNetSignalAdd* cmdAddNetSignal =
      new CmdNetSignalAdd(mProject.getCircuit(), *netclass);
  execNewChildCmd(cmdAddNetSignal);
  return cmdAddNetSignal->getNetSignal();
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb

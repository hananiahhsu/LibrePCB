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
#include "boardclipboarddatabuilder.h"

#include "boardnetsegmentsplitter.h"

#include <librepcb/common/geometry/polygon.h>
#include <librepcb/common/graphics/graphicslayer.h>
#include <librepcb/project/boards/board.h>
#include <librepcb/project/boards/boardselectionquery.h>
#include <librepcb/project/boards/items/bi_footprintpad.h>
#include <librepcb/project/boards/items/bi_hole.h>
#include <librepcb/project/boards/items/bi_netline.h>
#include <librepcb/project/boards/items/bi_netpoint.h>
#include <librepcb/project/boards/items/bi_netsegment.h>
#include <librepcb/project/boards/items/bi_plane.h>
#include <librepcb/project/boards/items/bi_polygon.h>
#include <librepcb/project/boards/items/bi_stroketext.h>
#include <librepcb/project/boards/items/bi_via.h>
#include <librepcb/project/circuit/netsignal.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

BoardClipboardDataBuilder::BoardClipboardDataBuilder(Board& board) noexcept
  : mBoard(board) {
}

BoardClipboardDataBuilder::~BoardClipboardDataBuilder() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

std::unique_ptr<BoardClipboardData> BoardClipboardDataBuilder::generate(
    const Point& cursorPos) const noexcept {
  std::unique_ptr<BoardClipboardData> data(
      new BoardClipboardData(mBoard.getUuid(), cursorPos));

  // get all selected items
  std::unique_ptr<BoardSelectionQuery> query(mBoard.createSelectionQuery());
  query->addSelectedVias();
  query->addSelectedNetLines();
  query->addSelectedPlanes();
  query->addSelectedPolygons();
  query->addSelectedBoardStrokeTexts();
  query->addSelectedHoles();

  // add (splitted) net segments including netpoints, netlines and netlabels
  foreach (BI_NetSegment* netsegment, mBoard.getNetSegments()) {
    BoardNetSegmentSplitter splitter;
    foreach (BI_Via* via, query->getVias()) {
      if (&via->getNetSegment() == netsegment) {
        splitter.addVia(via);
      }
    }
    foreach (BI_NetLine* netline, query->getNetLines()) {
      if (&netline->getNetSegment() == netsegment) {
        splitter.addNetLine(netline);
      }
    }

    foreach (const BoardNetSegmentSplitter::Segment& seg, splitter.split()) {
      std::shared_ptr<BoardClipboardData::NetSegment> newSegment =
          std::make_shared<BoardClipboardData::NetSegment>(
              netsegment->getNetSignal().getName());
      data->getNetSegments().append(newSegment);

      QHash<BI_NetLineAnchor*, std::shared_ptr<BoardClipboardData::NetPoint>>
          replacedNetPoints;
      foreach (BI_NetLineAnchor* anchor, seg.anchors) {
        if (BI_NetPoint* np = dynamic_cast<BI_NetPoint*>(anchor)) {
          newSegment->points.append(
              std::make_shared<BoardClipboardData::NetPoint>(
                  np->getUuid(), np->getPosition()));
        } else if (BI_Via* via = dynamic_cast<BI_Via*>(anchor)) {
          if (query->getVias().contains(via)) {
            newSegment->vias.append(std::make_shared<BoardClipboardData::Via>(
                via->getUuid(), via->getPosition(), via->getShape(),
                via->getSize(), via->getDrillDiameter()));
          } else {
            // Via will not be copied, thus replacing it by a netpoint
            std::shared_ptr<BoardClipboardData::NetPoint> np =
                std::make_shared<BoardClipboardData::NetPoint>(
                    Uuid::createRandom(), via->getPosition());
            replacedNetPoints.insert(via, np);
            newSegment->points.append(np);
          }
        } else if (BI_FootprintPad* pad =
                       dynamic_cast<BI_FootprintPad*>(anchor)) {
          // Pad will not be copied, thus replacing it by a netpoint
          std::shared_ptr<BoardClipboardData::NetPoint> np =
              std::make_shared<BoardClipboardData::NetPoint>(
                  Uuid::createRandom(), pad->getPosition());
          replacedNetPoints.insert(pad, np);
          newSegment->points.append(np);
        }
      }
      foreach (BI_NetLine* netline, seg.netlines) {
        std::shared_ptr<BoardClipboardData::NetLine> copy =
            std::make_shared<BoardClipboardData::NetLine>(
                netline->getUuid(), netline->getLayer().getName(),
                netline->getWidth());
        if (BI_NetPoint* netpoint =
                dynamic_cast<BI_NetPoint*>(&netline->getStartPoint())) {
          copy->startJunction = netpoint->getUuid();
        } else if (BI_Via* via =
                       dynamic_cast<BI_Via*>(&netline->getStartPoint())) {
          if (auto np = replacedNetPoints.value(via)) {
            copy->startJunction = np->uuid;
          } else {
            copy->startVia = via->getUuid();
          }
        } else if (BI_FootprintPad* pad = dynamic_cast<BI_FootprintPad*>(
                       &netline->getStartPoint())) {
          if (auto np = replacedNetPoints.value(pad)) {
            copy->startJunction = np->uuid;
          } else {
            Q_ASSERT(false);
          }
        } else {
          Q_ASSERT(false);
        }
        if (BI_NetPoint* netpoint =
                dynamic_cast<BI_NetPoint*>(&netline->getEndPoint())) {
          copy->endJunction = netpoint->getUuid();
        } else if (BI_Via* via =
                       dynamic_cast<BI_Via*>(&netline->getEndPoint())) {
          if (auto np = replacedNetPoints.value(via)) {
            copy->endJunction = np->uuid;
          } else {
            copy->endVia = via->getUuid();
          }
        } else if (BI_FootprintPad* pad = dynamic_cast<BI_FootprintPad*>(
                       &netline->getEndPoint())) {
          if (auto np = replacedNetPoints.value(pad)) {
            copy->endJunction = np->uuid;
          } else {
            Q_ASSERT(false);
          }
        } else {
          Q_ASSERT(false);
        }
        newSegment->lines.append(copy);
      }
    }
  }

  // add planes
  foreach (BI_Plane* plane, query->getPlanes()) {
    std::shared_ptr<BoardClipboardData::Plane> newPlane =
        std::make_shared<BoardClipboardData::Plane>(
            plane->getUuid(), *plane->getLayerName(),
            *plane->getNetSignal().getName(), plane->getOutline(),
            plane->getMinWidth(), plane->getMinClearance(),
            plane->getKeepOrphans(), plane->getPriority(),
            plane->getConnectStyle());
    data->getPlanes().append(newPlane);
  }

  // add polygons
  foreach (BI_Polygon* polygon, query->getPolygons()) {
    data->getPolygons().append(
        std::make_shared<Polygon>(polygon->getPolygon()));
  }

  // add stroke texts
  foreach (BI_StrokeText* text, query->getStrokeTexts()) {
    data->getStrokeTexts().append(
        std::make_shared<StrokeText>(text->getText()));
  }

  // add holes
  foreach (BI_Hole* hole, query->getHoles()) {
    data->getHoles().append(std::make_shared<Hole>(hole->getHole()));
  }

  return data;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb

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
#include "boardnetsegmentsplitter.h"

#include <librepcb/common/toolbox.h>
#include <librepcb/project/boards/items/bi_netline.h>
#include <librepcb/project/boards/items/bi_netpoint.h>
#include <librepcb/project/boards/items/bi_via.h>

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

BoardNetSegmentSplitter::BoardNetSegmentSplitter() noexcept
  : mVias(), mNetLines() {
}

BoardNetSegmentSplitter::~BoardNetSegmentSplitter() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

QList<BoardNetSegmentSplitter::Segment> BoardNetSegmentSplitter::split() const
    noexcept {
  QList<Segment> segments;

  // Split netsegment by anchors and lines
  QList<BI_NetLine*> netlines = mNetLines;
  QList<BI_Via*>     vias     = mVias;
  while (netlines.count() > 0) {
    Segment                  segment;
    QList<BI_NetLineAnchor*> processedAnchors;
    findConnectedLinesAndPoints(netlines.first()->getStartPoint(),
                                processedAnchors, segment.anchors,
                                segment.netlines, vias, netlines);
    segments.append(segment);
  }
  Q_ASSERT(netlines.isEmpty());

  // Add remaining vias as separate segments
  foreach (BI_Via* via, vias) {
    Segment segment;
    segment.anchors.append(via);
    segments.append(segment);
  }

  return segments;
}

void BoardNetSegmentSplitter::findConnectedLinesAndPoints(
    BI_NetLineAnchor& anchor, QList<BI_NetLineAnchor*>& processedAnchors,
    QList<BI_NetLineAnchor*>& anchors, QList<BI_NetLine*>& netlines,
    QList<BI_Via*>& availableVias, QList<BI_NetLine*>& availableNetLines) const
    noexcept {
  Q_ASSERT(!processedAnchors.contains(&anchor));
  processedAnchors.append(&anchor);

  bool isRemovedVia = false;
  if (BI_Via* anchorVia = dynamic_cast<BI_Via*>(&anchor)) {
    if (mVias.contains(anchorVia)) {
      availableVias.removeOne(anchorVia);
    } else {
      isRemovedVia = true;
    }
  }

  Q_ASSERT(!anchors.contains(&anchor));
  anchors.append(&anchor);

  GraphicsLayer* layer = nullptr;
  foreach (BI_NetLine* line, anchor.getNetLines()) {
    if (availableNetLines.contains(line) && (!netlines.contains(line)) &&
        ((!isRemovedVia) || (!layer) || (&line->getLayer() == layer))) {
      layer = &line->getLayer();
      netlines.append(line);
      availableNetLines.removeOne(line);
      BI_NetLineAnchor* p2 = line->getOtherPoint(anchor);
      Q_ASSERT(p2);
      if (!processedAnchors.contains(p2)) {
        findConnectedLinesAndPoints(*p2, processedAnchors, anchors, netlines,
                                    availableVias, availableNetLines);
      }
    }
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb

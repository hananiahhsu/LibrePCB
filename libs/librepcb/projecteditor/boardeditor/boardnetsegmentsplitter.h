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

#ifndef LIBREPCB_PROJECT_EDITOR_BOARDNETSEGMENTSPLITTER_H
#define LIBREPCB_PROJECT_EDITOR_BOARDNETSEGMENTSPLITTER_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/common/units/all_length_units.h>
#include <librepcb/common/uuid.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace project {

class BI_NetSegment;
class BI_NetPoint;
class BI_NetLine;
class BI_NetLineAnchor;
class BI_Via;

namespace editor {

/*******************************************************************************
 *  Class BoardNetSegmentSplitter
 ******************************************************************************/

/**
 * @brief The BoardNetSegmentSplitter class
 */
class BoardNetSegmentSplitter final {
public:
  // Types
  struct Segment {
    QList<BI_NetLineAnchor*> anchors;
    QList<BI_NetLine*>       netlines;
  };

  // Constructors / Destructor
  BoardNetSegmentSplitter() noexcept;
  BoardNetSegmentSplitter(const BoardNetSegmentSplitter& other) = delete;
  ~BoardNetSegmentSplitter() noexcept;

  // General Methods
  void addVia(BI_Via* via) noexcept {
    Q_ASSERT(!mVias.contains(via));
    mVias.append(via);
  }
  void addNetLine(BI_NetLine* netline) noexcept {
    Q_ASSERT(!mNetLines.contains(netline));
    mNetLines.append(netline);
  }
  QList<Segment> split() const noexcept;

  // Operator Overloadings
  BoardNetSegmentSplitter& operator=(const BoardNetSegmentSplitter& rhs) =
      delete;

private:  // Methods
  void findConnectedLinesAndPoints(BI_NetLineAnchor&         anchor,
                                   QList<BI_NetLineAnchor*>& processedAnchors,
                                   QList<BI_NetLineAnchor*>& anchors,
                                   QList<BI_NetLine*>&       netlines,
                                   QList<BI_Via*>&           availableVias,
                                   QList<BI_NetLine*>& availableNetLines) const
      noexcept;

private:  // Data
  QList<BI_Via*>     mVias;
  QList<BI_NetLine*> mNetLines;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb

#endif

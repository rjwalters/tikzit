/*
    TikZiT - a GUI diagram editor for TikZ
    Copyright (C) 2018 Aleks Kissinger

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef TIKZPARSERDEFS_H
#define TIKZPARSERDEFS_H

#define YY_NO_UNISTD_H 1

#include "graphelementproperty.h"
#include "graphelementdata.h"
#include "node.h"
#include "tikzassembler.h"

#include <QString>
#include <QRectF>
#include <QDebug>

struct noderef {
    Node *node;
    char *anchor;
    bool cycle;
    bool loop;
    QPointF *coord;  // for resolved coordinates from ++ and coordinate calculations
};

inline double parseDimension(const QString &s)
{
    // Parse a dimension value, converting to cm.
    // Supports: plain numbers (assumed cm), Xpt, Xcm, Xmm, Xin, Xem, Xex
    QString trimmed = s.trimmed();
    double val = 0;
    QString unit;

    // Extract numeric prefix and unit suffix
    int i = 0;
    // Allow leading minus/plus
    if (i < trimmed.length() && (trimmed[i] == '-' || trimmed[i] == '+')) i++;
    while (i < trimmed.length() && (trimmed[i].isDigit() || trimmed[i] == '.')) i++;

    bool ok;
    val = trimmed.left(i).toDouble(&ok);
    if (!ok) return 0;

    unit = trimmed.mid(i).trimmed().toLower();

    if (unit.isEmpty() || unit == "cm") return val;
    if (unit == "pt") return val / 28.45;
    if (unit == "mm") return val / 10.0;
    if (unit == "in") return val * 2.54;
    if (unit == "em") return val * 0.35;  // approximate
    if (unit == "ex") return val * 0.15;  // approximate
    return val; // unknown unit, treat as cm
}

inline int isatty(int) { return 0; }

#endif // TIKZPARSERDEFS_H

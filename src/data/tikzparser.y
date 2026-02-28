%{
/*!
 * \file tikzparser.y
 *
 * The parser for tikz input.
 */

/*
 * Copyright 2010       Chris Heunen
 * Copyright 2010-2017  Aleks Kissinger
 * Copyright 2013       K. Johan Paulsson
 * Copyright 2013       Alex Merry <dev@randomguy3.me.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "tikzparserdefs.h"
%}

/* we use features added to bison 2.4 */
%require "2.3"

%error-verbose
/* enable maintaining locations for better error messages */
%locations
/* the name of the header file */
/*%defines "common/tikzparser.h"*/
/* make it re-entrant (no global variables) */
%pure-parser
/* We use a pure (re-entrant) lexer.  This means yylex
   will take a void* (opaque) type to maintain its state */
%lex-param {void *scanner}
/* Since this parser is also pure, yyparse needs to take
   that lexer state as an argument */
%parse-param {void *scanner}

/* possible data types for semantic values */
%union {
    char *str;
    GraphElementProperty *prop;
    GraphElementData *data;
    Node *node;
    QPointF *pt;
    struct noderef noderef;
}

%{
#include "node.h"
#include "edge.h"
#include "graphelementdata.h"
#include "graphelementproperty.h"

#include "tikzlexer.h"
#include "tikzassembler.h"
#include <QRegularExpression>
/* the assembler (used by this parser) is stored in the lexer
   state as "extra" data */
#define assembler yyget_extra(scanner)

/* pass errors off to the assembler */
void yyerror(YYLTYPE *yylloc, void *scanner, const char *str) {
    assembler->reportError(QString::fromUtf8(str), yylloc->first_line);
    qDebug() << "\nparse error: " << str << " line:" << yylloc->first_line;
}
%}

/* yyloc is set up with first_column = last_column = 1 by default;
   however, it makes more sense to think of us being "before the
   start of the line" before we parse anything */
%initial-action {
	yylloc.first_column = yylloc.last_column = 0;
}


%token BEGIN_TIKZPICTURE_CMD "\\begin{tikzpicture}"
%token END_TIKZPICTURE_CMD "\\end{tikzpicture}"
%token TIKZSTYLE_CMD "\\tikzstyle"
%token TIKZSET_CMD "\\tikzset"
%token BEGIN_PGFONLAYER_CMD "\\begin{pgfonlayer}"
%token END_PGFONLAYER_CMD "\\end{pgfonlayer}"
%token DRAW_CMD "\\draw"
%token NODE_CMD "\\node"
%token PATH_CMD "\\path"
%token RECTANGLE "rectangle"
%token NODE "node"
%token AT "at"
%token TO "to"
%token CYCLE "cycle"
%token MINUSMINUS "--"
%token HV_LINE "-|"
%token VH_LINE "|-"
%token PLUSPLUS "++"
%token BEGIN_SCOPE_CMD "\\begin{scope}"
%token END_SCOPE_CMD "\\end{scope}"
%token SEMICOLON ";"
%token COMMA ","

%token LEFTPARENTHESIS "("
%token RIGHTPARENTHESIS ")"
%token LEFTBRACKET "["
%token RIGHTBRACKET "]"
%token FULLSTOP "."
%token EQUALS "="
%token <pt> TCOORD "coordinate"
%token <str> PROPSTRING "key/value string"
%token <str> REFSTRING "string"
%token <str> DELIMITEDSTRING "{-delimited string"

%token UNKNOWN_BEGIN_CMD "unknown \\begin command"
%token UNKNOWN_END_CMD "unknown \\end command"
%token UNKNOWN_CMD "unknown latex command"
%token UNKNOWN_STR "unknown string"
%token UNCLOSED_DELIM_STR "unclosed {-delimited string"

%type<str>   nodename
%type<str>   optnodename
%type<str>   optanchor
%type<str>   val
%type<prop>    property
%type<data>    extraproperties
%type<data>    properties
%type<data>    optproperties
%type<node>    optedgenode
%type<noderef> noderef
%type<noderef> optnoderef
%type<pt>      optat

%%


tikz: tikzstyles | tikzpicture;

tikzstyles: tikzstyles tikzstyle | ;
tikzstyle:
    "\\tikzstyle" DELIMITEDSTRING "=" "[" properties "]"
    {
        if (assembler->isTikzStyles()) {
            assembler->tikzStyles()->addStyle(QString::fromUtf8($2), $5);
        }
    }
    | "\\tikzset" DELIMITEDSTRING
    {
        if (assembler->isTikzStyles()) {
            QString content($2);
            free($2);

            // Parse "NAME/.style={PROPS}" from the delimited string content
            int styleIdx = content.indexOf("/.style=");
            if (styleIdx != -1) {
                QString name = content.left(styleIdx).trimmed();
                QString propsStr = content.mid(styleIdx + 8).trimmed(); // skip "/.style="

                // Strip outer braces from property list if present: {PROPS} -> PROPS
                if (propsStr.startsWith("{") && propsStr.endsWith("}")) {
                    propsStr = propsStr.mid(1, propsStr.length() - 2);
                }

                // Parse properties from comma-separated list, handling nested braces
                GraphElementData *data = new GraphElementData();
                int depth = 0;
                int start = 0;
                for (int i = 0; i <= propsStr.length(); ++i) {
                    QChar c = (i < propsStr.length()) ? propsStr[i] : QChar(',');
                    if (c == '{') { depth++; }
                    else if (c == '}') { depth--; }
                    else if ((c == ',' && depth == 0) || i == propsStr.length()) {
                        QString token = propsStr.mid(start, i - start).trimmed();
                        if (!token.isEmpty()) {
                            int eqIdx = token.indexOf('=');
                            if (eqIdx != -1) {
                                QString key = token.left(eqIdx).trimmed();
                                QString val = token.mid(eqIdx + 1).trimmed();
                                // Strip braces from value if present
                                if (val.startsWith("{") && val.endsWith("}")) {
                                    val = val.mid(1, val.length() - 2);
                                }
                                data->add(GraphElementProperty(key, val));
                            } else {
                                data->add(GraphElementProperty(token));
                            }
                        }
                        start = i + 1;
                    }
                }

                assembler->tikzStyles()->addStyle(name, data);
            }
        }
    }

tikzpicture: "\\begin{tikzpicture}" optproperties tikzcmds "\\end{tikzpicture}"
    {
        if (assembler->isGraph() && $2) {
            assembler->graph()->setData($2);
		}
	};
tikzcmds: tikzcmds tikzcmd | ;
tikzcmd: node | edge | boundingbox | ignore;

ignore: "\\begin{pgfonlayer}" DELIMITEDSTRING
    | "\\end{pgfonlayer}"
    | "\\begin{scope}" optproperties
    | "\\end{scope}";

optproperties:
	"[" "]"
	{ $$ = 0; }
	| "[" properties "]"
	{ $$ = $2; }
	| { $$ = 0; };
properties: extraproperties property
	{
        $1->add(*$2);
        delete $2;
        $$ = $1;
	}
	| extraproperties property ","
	{
        $1->add(*$2);
        delete $2;
        $$ = $1;
	};
extraproperties:
	extraproperties property ","
	{
        $1->add(*$2);
        delete $2;
        $$ = $1;
	}
    | { $$ = new GraphElementData(); };
property:
	val "=" val
    {
        GraphElementProperty *p = new GraphElementProperty(QString::fromUtf8($1),QString::fromUtf8($3));
        free($1);
        free($3);
        $$ = p;
    }
	| val
    {
        GraphElementProperty *a = new GraphElementProperty(QString::fromUtf8($1));
        free($1);
        $$ = a;
    };
val: PROPSTRING { $$ = $1; } | DELIMITEDSTRING { $$ = $1; };

nodename: "(" REFSTRING ")" { $$ = $2; };
optnodename: nodename { $$ = $1; } | { $$ = 0; };
optat: "at" TCOORD { $$ = $2; }
    | "at" noderef {
        if ($2.node) {
            QPointF pos = $2.node->point();
            if ($2.anchor) {
                pos += assembler->anchorOffset($2.node, QString::fromUtf8($2.anchor));
                free($2.anchor);
            }
            if ($2.coord) delete $2.coord;
            $$ = new QPointF(pos);
        } else if ($2.coord) {
            if ($2.anchor) free($2.anchor);
            $$ = $2.coord;
        } else {
            if ($2.anchor) free($2.anchor);
            if ($2.coord) delete $2.coord;
            $$ = new QPointF(0, 0);
        }
    }
    | { $$ = 0; };
node: "\\node" optproperties optnodename optat DELIMITEDSTRING ";"
	{
        Node *node = new Node();

        if ($2) {
            node->setData($2);
        }

        if ($3) {
            node->setName(QString::fromUtf8($3));
            free($3);
        } else {
            node->setName(assembler->graph()->freshNodeName());
        }

        node->setLabel(QString::fromUtf8($5));
        free($5);

        if ($4) {
            node->setPoint(*$4);
            delete $4;
        }
        // If no position given, stays at (0,0); resolved in post-processing

        assembler->graph()->addNode(node);
        assembler->addNodeToMap(node);
	};

optanchor:  { $$ = 0; } | "." REFSTRING { $$ = $2; };
noderef: "(" REFSTRING optanchor ")"
	{
        QString name = QString::fromUtf8($2);
        $$.node = assembler->nodeWithName(name);
        $$.coord = 0;
        $$.loop = false;
        $$.cycle = false;

        if (!$$.node) {
            // Check for coordinate calculation: "a -| b" or "a |- b"
            int hvIdx = name.indexOf(QStringLiteral(" -| "));
            int vhIdx = name.indexOf(QStringLiteral(" |- "));
            if (hvIdx >= 0) {
                QString refA = name.left(hvIdx).trimmed();
                QString refB = name.mid(hvIdx + 4).trimmed();
                QString anchorB = $3 ? QString::fromUtf8($3) : QString();
                $$.coord = new QPointF(assembler->resolveCoordCalc(refA, QString(), refB, anchorB, true));
            } else if (vhIdx >= 0) {
                QString refA = name.left(vhIdx).trimmed();
                QString refB = name.mid(vhIdx + 4).trimmed();
                QString anchorB = $3 ? QString::fromUtf8($3) : QString();
                $$.coord = new QPointF(assembler->resolveCoordCalc(refA, QString(), refB, anchorB, false));
            }
            // Check for shifted coordinate: "[xshift=X,yshift=Y]refname"
            else if (name.startsWith(QLatin1String("["))) {
                int bracketEnd = name.indexOf(QLatin1Char(']'));
                if (bracketEnd >= 0) {
                    QString shifts = name.mid(1, bracketEnd - 1);
                    QString refName = name.mid(bracketEnd + 1).trimmed();
                    Node *ref = assembler->nodeWithName(refName);
                    if (ref) {
                        QPointF pos = ref->point();
                        if ($3) pos += assembler->anchorOffset(ref, QString::fromUtf8($3));
                        // Parse xshift/yshift
                        QRegularExpression xshiftRe(QStringLiteral("xshift\\s*=\\s*([\\d.\\-]+\\s*(?:pt|cm|mm|in)?)"));
                        QRegularExpression yshiftRe(QStringLiteral("yshift\\s*=\\s*([\\d.\\-]+\\s*(?:pt|cm|mm|in)?)"));
                        QRegularExpressionMatch xm = xshiftRe.match(shifts);
                        QRegularExpressionMatch ym = yshiftRe.match(shifts);
                        if (xm.hasMatch()) {
                            pos.setX(pos.x() + parseDimension(xm.captured(1)));
                        }
                        if (ym.hasMatch()) {
                            pos.setY(pos.y() + parseDimension(ym.captured(1)));
                        }
                        $$.coord = new QPointF(pos);
                    }
                }
            }
        }

        free($2);
        $$.anchor = $3;
	};
optnoderef:
    noderef { $$ = $1; }
    | "(" ")" { $$.node = 0; $$.anchor = 0; $$.loop = true; $$.cycle = false; $$.coord = 0; }
    | "cycle" { $$.node = 0; $$.anchor = 0; $$.loop = false; $$.cycle = true; $$.coord = 0; }
    | "++" TCOORD {
        QPointF pos = assembler->currentDrawPos() + *$2;
        delete $2;
        $$.node = assembler->createSyntheticNode(pos);
        $$.anchor = 0;
        $$.loop = false;
        $$.cycle = false;
        $$.coord = 0;
    }
optedgenode:
	{ $$ = 0; }
	| "node" optproperties DELIMITEDSTRING
    {
        $$ = new Node();
        if ($2)
            $$->setData($2);
        $$->setLabel(QString::fromUtf8($3));
        free($3);
	}

edgesource: optproperties noderef {
        Node *src = $2.node;
        if (!src && $2.coord) {
            src = assembler->createSyntheticNode(*$2.coord);
            delete $2.coord;
        }
        assembler->setCurrentEdgeSource(src);
        if (src) {
            assembler->setCurrentDrawPos(src->point());
        }
        if ($2.anchor) {
            assembler->setCurrentEdgeSourceAnchor(QString::fromUtf8($2.anchor));
            free($2.anchor);
        } else {
            assembler->setCurrentEdgeSourceAnchor(QString());
        }
        assembler->setCurrentEdgeData($1);
    }

optedgetargets: edgetarget optedgetargets |

connector: "to" | "--" | "-|" | "|-";

edgetarget: connector optproperties optedgenode optnoderef optedgenode {
        Node *s = assembler->currentEdgeSource();
        Node *t;

        if ($4.loop) {
            t = assembler->currentEdgeSource();
        } else if ($4.cycle) {
            t = assembler->currentPathSource();
            if (!t) t = s;
        } else if ($4.node) {
            t = $4.node;
        } else if ($4.coord) {
            t = assembler->createSyntheticNode(*$4.coord);
            delete $4.coord;
        } else {
            t = 0;
        }

        if (s != 0 && t != 0) { // if source or target don't exist, quietly ignore edge
            Edge *e = new Edge(s, t);
            assembler->setCurrentEdgeSource(t);
            assembler->setCurrentDrawPos(t->point());

            if (!assembler->currentEdgeSourceAnchor().isEmpty()) {
                e->setSourceAnchor(assembler->currentEdgeSourceAnchor());
            }

            if ($4.anchor) {
                QString a(QString::fromUtf8($4.anchor));
                free($4.anchor);
                e->setTargetAnchor(a);
                assembler->setCurrentEdgeSourceAnchor(a);
            } else {
                assembler->setCurrentEdgeSourceAnchor(QString());
            }

            Node *en = $3 ? $3 : $5;
            if (en) e->setEdgeNode(en);

            GraphElementData *cd = assembler->currentEdgeData();
            if ($2) {
                if (cd) $2->mergeData(cd);
                 e->setData($2);
            } else {
                if (cd) e->setData(cd->copy());
            }
            e->setAttributesFromData();
            assembler->addEdge(e);
        }
    }


edge: "\\draw" edgesource edgetarget optedgetargets ";"
	{
        assembler->finishCurrentPath();
	};

ignoreprop: val | val "=" val;
ignoreprops: ignoreprop ignoreprops | ;
optignoreprops: "[" ignoreprops "]";
boundingbox:
    "\\path" optignoreprops TCOORD "rectangle" TCOORD ";"
	{
        assembler->graph()->setBbox(QRectF(*$3, *$5));
        delete $3;
        delete $5;
	};

/* vi:ft=yacc:noet:ts=4:sts=4:sw=4
*/

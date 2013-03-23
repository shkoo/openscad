/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "cgaladvnode.h"
#include "module.h"
#include "context.h"
#include "builtin.h"
#include "PolySetEvaluator.h"
#include <sstream>
#include <assert.h>
#include <boost/assign/std/vector.hpp>
#include <boost/algorithm/string.hpp>
#include "printutils.h"
using namespace boost::assign; // bring 'operator+=()' into scope

class CgaladvModule : public AbstractModule
{
public:
	cgaladv_type_e type;
	CgaladvModule(cgaladv_type_e type) : type(type) { }
	virtual AbstractNode *evaluate(const Context *ctx, const ModuleInstantiation *inst) const;
};

AbstractNode *CgaladvModule::evaluate(const Context *ctx, const ModuleInstantiation *inst) const
{
	CgaladvNode *node = new CgaladvNode(inst, type);

	std::vector<std::string> argnames;
	std::vector<Expression*> argexpr;

	if (type == MINKOWSKI)
		argnames += "convexity";

	if (type == GLIDE)
		argnames += "path", "convexity";

	if (type == SUBDIV)
		argnames += "level", "type", "convexity";

	if (type == RESIZE)
		argnames += "newsize", "auto";

	Context c(ctx);
	c.args(argnames, argexpr, inst->argnames, inst->argvalues);

	Value convexity, path;

	if (type == MINKOWSKI) {
		convexity = c.lookup_variable("convexity", true);
	}

	if (type == GLIDE) {
		convexity = c.lookup_variable("convexity", true);
		path = c.lookup_variable("path", false);
	}

	if (type == SUBDIV) {
		Value subdiv_level = c.lookup_variable("level", true);
		Value subdiv_typeval = c.lookup_variable("type", false);

		// enable subdiv("loop",1) or subdiv(1,"loop")
		if ( subdiv_level.type() == Value::STRING )
			if ( subdiv_typeval.type() == Value::NUMBER )
				std::swap( subdiv_level, subdiv_typeval );


		if (subdiv_level.isUndefined())	node->subdiv_level = 1;
		else node->subdiv_level = (int)subdiv_level.toDouble();
		if (node->subdiv_level < 0) {
			PRINT("WARNING: Subdivision cannot be less than 0. Setting to 0.");
			node->subdiv_level = 0;
		}

		std::string subdiv_type = "catmullclark";
		if (!subdiv_typeval.isUndefined()) subdiv_type = subdiv_typeval.toString();
		boost::algorithm::to_lower( subdiv_type );
		if ( subdiv_type == "catmullclark" || subdiv_type == "catmull clark" )
			node->subdiv_type = SUBDIV_CATMULL_CLARK;
		else if ( subdiv_type == "loop" )
			node->subdiv_type = SUBDIV_LOOP;
		else if ( subdiv_type == "doosabin" || subdiv_type == "doo sabin" )
			node->subdiv_type = SUBDIV_DOO_SABIN;
		else if ( subdiv_type == "sqrt3" || subdiv_type == "sqrt 3" )
			node->subdiv_type = SUBDIV_SQRT3;
		else {
			PRINTB("WARNING: unknown subdivision type %s",subdiv_type);
			PRINT("WARNING: setting to CatmullClark");
			node->subdiv_type = SUBDIV_CATMULL_CLARK;
		}

		convexity = c.lookup_variable("convexity", true);
	}

	if (type == RESIZE) {
		Value ns = c.lookup_variable("newsize");
		node->newsize << 0,0,0;
		if ( ns.type() == Value::VECTOR ) {
			Value::VectorType vs = ns.toVector();
			if ( vs.size() >= 1 ) node->newsize[0] = vs[0].toDouble();
			if ( vs.size() >= 2 ) node->newsize[1] = vs[1].toDouble();
			if ( vs.size() >= 3 ) node->newsize[2] = vs[2].toDouble();
		}
		Value autosize = c.lookup_variable("auto");
		node->autosize << false, false, false;
		if ( autosize.type() == Value::VECTOR ) {
			Value::VectorType va = autosize.toVector();
			if ( va.size() >= 1 ) node->autosize[0] = va[0].toBool();
			if ( va.size() >= 2 ) node->autosize[1] = va[1].toBool();
			if ( va.size() >= 3 ) node->autosize[2] = va[2].toBool();
		}
		else if ( autosize.type() == Value::BOOL ) {
			node->autosize << true, true, true;
		}
	}

	node->convexity = (int)convexity.toDouble();
	node->path = path;

	std::vector<AbstractNode *> evaluatednodes = inst->evaluateChildren();
	node->children.insert(node->children.end(), evaluatednodes.begin(), evaluatednodes.end());

	return node;
}

PolySet *CgaladvNode::evaluate_polyset(PolySetEvaluator *ps) const
{
	return ps->evaluatePolySet(*this);
}

std::string CgaladvNode::name() const
{
	switch (this->type) {
	case MINKOWSKI:
		return "minkowski";
		break;
	case GLIDE:
		return "glide";
		break;
	case SUBDIV:
		return "subdiv";
		break;
	case HULL:
		return "hull";
		break;
	case RESIZE:
		return "resize";
		break;
	default:
		assert(false);
	}
}

std::string CgaladvNode::toString() const
{
	std::stringstream stream;

	stream << this->name();
	switch (type) {
	case MINKOWSKI:
		stream << "(convexity = " << this->convexity << ")";
		break;
	case GLIDE:
		stream << "(path = " << this->path << ", convexity = " << this->convexity << ")";
		break;
	case SUBDIV:
		stream << "(type = " << this->subdiv_type << ", level = " << this->subdiv_level << ", convexity = " << this->convexity << ")";
		break;
	case HULL:
		stream << "()";
		break;
	case RESIZE:
		stream << "(newsize = ["
		  << this->newsize[0] << "," << this->newsize[1] << "," << this->newsize[2] << "]"
		  << ", auto = ["
		  << this->autosize[0] << "," << this->autosize[1] << "," << this->autosize[2] << "]"
		  << ")";
		break;
	default:
		assert(false);
	}

	return stream.str();
}

void register_builtin_cgaladv()
{
	Builtins::init("minkowski", new CgaladvModule(MINKOWSKI));
	Builtins::init("glide", new CgaladvModule(GLIDE));
	Builtins::init("subdiv", new CgaladvModule(SUBDIV));
	Builtins::init("hull", new CgaladvModule(HULL));
	Builtins::init("resize", new CgaladvModule(RESIZE));
}

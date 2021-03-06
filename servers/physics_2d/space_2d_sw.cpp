/*************************************************************************/
/*  space_2d_sw.cpp                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include "space_2d_sw.h"
#include "collision_solver_2d_sw.h"
#include "physics_2d_server_sw.h"


bool Physics2DDirectSpaceStateSW::intersect_ray(const Vector2& p_from, const Vector2& p_to,RayResult &r_result,const Set<RID>& p_exclude,uint32_t p_user_mask) {


	ERR_FAIL_COND_V(space->locked,false);

	Vector2 begin,end;
	Vector2 normal;
	begin=p_from;
	end=p_to;
	normal=(end-begin).normalized();


	int amount = space->broadphase->cull_segment(begin,end,space->intersection_query_results,Space2DSW::INTERSECTION_QUERY_MAX,space->intersection_query_subindex_results);

	//todo, create another array tha references results, compute AABBs and check closest point to ray origin, sort, and stop evaluating results when beyond first collision

	bool collided=false;
	Vector2 res_point,res_normal;
	int res_shape;
	const CollisionObject2DSW *res_obj;
	real_t min_d=1e10;


	for(int i=0;i<amount;i++) {

		if (space->intersection_query_results[i]->get_type()==CollisionObject2DSW::TYPE_AREA)
			continue; //ignore area

		if (p_exclude.has( space->intersection_query_results[i]->get_self()))
			continue;

		const CollisionObject2DSW *col_obj=space->intersection_query_results[i];

		int shape_idx=space->intersection_query_subindex_results[i];
		Matrix32 inv_xform = col_obj->get_shape_inv_transform(shape_idx) * col_obj->get_inv_transform();

		Vector2 local_from = inv_xform.xform(begin);
		Vector2 local_to = inv_xform.xform(end);

		/*local_from = col_obj->get_inv_transform().xform(begin);
		local_from = col_obj->get_shape_inv_transform(shape_idx).xform(local_from);

		local_to = col_obj->get_inv_transform().xform(end);
		local_to = col_obj->get_shape_inv_transform(shape_idx).xform(local_to);*/

		const Shape2DSW *shape = col_obj->get_shape(shape_idx);

		Vector2 shape_point,shape_normal;


		if (shape->intersect_segment(local_from,local_to,shape_point,shape_normal)) {


			//print_line("inters sgment!");
			Matrix32 xform = col_obj->get_transform() * col_obj->get_shape_transform(shape_idx);
			shape_point=xform.xform(shape_point);

			real_t ld = normal.dot(shape_point);


			if (ld<min_d) {

				min_d=ld;
				res_point=shape_point;
				res_normal=inv_xform.basis_xform_inv(shape_normal).normalized();
				res_shape=shape_idx;
				res_obj=col_obj;
				collided=true;
			}
		}

	}

	if (!collided)
		return false;


	r_result.collider_id=res_obj->get_instance_id();
	if (r_result.collider_id!=0)
		r_result.collider=ObjectDB::get_instance(r_result.collider_id);
	r_result.normal=res_normal;
	r_result.position=res_point;
	r_result.rid=res_obj->get_self();
	r_result.shape=res_shape;

	return true;

}


int Physics2DDirectSpaceStateSW::intersect_shape(const RID& p_shape, const Matrix32& p_xform,ShapeResult *r_results,int p_result_max,const Set<RID>& p_exclude,uint32_t p_user_mask) {

	if (p_result_max<=0)
		return 0;

	Shape2DSW *shape = static_cast<Physics2DServerSW*>(Physics2DServer::get_singleton())->shape_owner.get(p_shape);
	ERR_FAIL_COND_V(!shape,0);

	Rect2 aabb = p_xform.xform(shape->get_aabb());

	int amount = space->broadphase->cull_aabb(aabb,space->intersection_query_results,Space2DSW::INTERSECTION_QUERY_MAX,space->intersection_query_subindex_results);

	bool collided=false;
	int cc=0;

	for(int i=0;i<amount;i++) {

		if (cc>=p_result_max)
			break;

		if (space->intersection_query_results[i]->get_type()==CollisionObject2DSW::TYPE_AREA)
			continue; //ignore area

		if (p_exclude.has( space->intersection_query_results[i]->get_self()))
			continue;


		const CollisionObject2DSW *col_obj=space->intersection_query_results[i];
		int shape_idx=space->intersection_query_subindex_results[i];

		if (!CollisionSolver2DSW::solve_static(shape,p_xform,p_xform.affine_inverse(),col_obj->get_shape(shape_idx),col_obj->get_transform() * col_obj->get_shape_transform(shape_idx), col_obj->get_inv_transform() * col_obj->get_shape_inv_transform(shape_idx),NULL,NULL,NULL))
			continue;

		r_results[cc].collider_id=col_obj->get_instance_id();
		if (r_results[cc].collider_id!=0)
			r_results[cc].collider=ObjectDB::get_instance(r_results[cc].collider_id);
		r_results[cc].rid=col_obj->get_self();
		r_results[cc].shape=shape_idx;

		cc++;

	}

	return cc;

}

Physics2DDirectSpaceStateSW::Physics2DDirectSpaceStateSW() {


	space=NULL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////










void* Space2DSW::_broadphase_pair(CollisionObject2DSW *A,int p_subindex_A,CollisionObject2DSW *B,int p_subindex_B,void *p_self) {

	CollisionObject2DSW::Type type_A=A->get_type();
	CollisionObject2DSW::Type type_B=B->get_type();
	if (type_A>type_B) {

		SWAP(A,B);
		SWAP(p_subindex_A,p_subindex_B);
		SWAP(type_A,type_B);
	}

	Space2DSW *self = (Space2DSW*)p_self;

	if (type_A==CollisionObject2DSW::TYPE_AREA) {


		ERR_FAIL_COND_V(type_B!=CollisionObject2DSW::TYPE_BODY,NULL);
		Area2DSW *area=static_cast<Area2DSW*>(A);
		Body2DSW *body=static_cast<Body2DSW*>(B);

		AreaPair2DSW *area_pair = memnew(AreaPair2DSW(body,p_subindex_B,area,p_subindex_A) );

		return area_pair;
	} else {


		BodyPair2DSW *b = memnew( BodyPair2DSW((Body2DSW*)A,p_subindex_A,(Body2DSW*)B,p_subindex_B) );
		return b;

	}

	return NULL;
}

void Space2DSW::_broadphase_unpair(CollisionObject2DSW *A,int p_subindex_A,CollisionObject2DSW *B,int p_subindex_B,void *p_data,void *p_self) {



	Space2DSW *self = (Space2DSW*)p_self;
	Constraint2DSW *c = (Constraint2DSW*)p_data;
	memdelete(c);
}


const SelfList<Body2DSW>::List& Space2DSW::get_active_body_list() const {

	return active_list;
}
void Space2DSW::body_add_to_active_list(SelfList<Body2DSW>* p_body) {

	active_list.add(p_body);
}
void Space2DSW::body_remove_from_active_list(SelfList<Body2DSW>* p_body) {

	active_list.remove(p_body);

}

void Space2DSW::body_add_to_inertia_update_list(SelfList<Body2DSW>* p_body) {


	inertia_update_list.add(p_body);
}

void Space2DSW::body_remove_from_inertia_update_list(SelfList<Body2DSW>* p_body) {

	inertia_update_list.remove(p_body);
}

BroadPhase2DSW *Space2DSW::get_broadphase() {

	return broadphase;
}

void Space2DSW::add_object(CollisionObject2DSW *p_object) {

	ERR_FAIL_COND( objects.has(p_object) );
	objects.insert(p_object);
}

void Space2DSW::remove_object(CollisionObject2DSW *p_object) {

	ERR_FAIL_COND( !objects.has(p_object) );
	objects.erase(p_object);
}

const Set<CollisionObject2DSW*> &Space2DSW::get_objects() const {

	return objects;
}

void Space2DSW::body_add_to_state_query_list(SelfList<Body2DSW>* p_body) {

	state_query_list.add(p_body);
}
void Space2DSW::body_remove_from_state_query_list(SelfList<Body2DSW>* p_body) {

	state_query_list.remove(p_body);
}

void Space2DSW::area_add_to_monitor_query_list(SelfList<Area2DSW>* p_area) {

	monitor_query_list.add(p_area);
}
void Space2DSW::area_remove_from_monitor_query_list(SelfList<Area2DSW>* p_area) {

	monitor_query_list.remove(p_area);
}

void Space2DSW::area_add_to_moved_list(SelfList<Area2DSW>* p_area) {

	area_moved_list.add(p_area);
}

void Space2DSW::area_remove_from_moved_list(SelfList<Area2DSW>* p_area) {

	area_moved_list.remove(p_area);
}

const SelfList<Area2DSW>::List& Space2DSW::get_moved_area_list() const {

	return area_moved_list;
}


void Space2DSW::call_queries() {

	while(state_query_list.first()) {

		Body2DSW * b = state_query_list.first()->self();
		b->call_queries();
		state_query_list.remove(state_query_list.first());
	}

	while(monitor_query_list.first()) {

		Area2DSW * a = monitor_query_list.first()->self();
		a->call_queries();
		monitor_query_list.remove(monitor_query_list.first());
	}

}

void Space2DSW::setup() {


	while(inertia_update_list.first()) {
		inertia_update_list.first()->self()->update_inertias();
		inertia_update_list.remove(inertia_update_list.first());
	}


}

void Space2DSW::update() {

	broadphase->update();

}


void Space2DSW::set_param(Physics2DServer::SpaceParameter p_param, real_t p_value) {

	switch(p_param) {

		case Physics2DServer::SPACE_PARAM_CONTACT_RECYCLE_RADIUS: contact_recycle_radius=p_value; break;
		case Physics2DServer::SPACE_PARAM_CONTACT_MAX_SEPARATION: contact_max_separation=p_value; break;
		case Physics2DServer::SPACE_PARAM_BODY_MAX_ALLOWED_PENETRATION: contact_max_allowed_penetration=p_value; break;
		case Physics2DServer::SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_TRESHOLD: body_linear_velocity_sleep_treshold=p_value; break;
		case Physics2DServer::SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_TRESHOLD: body_angular_velocity_sleep_treshold=p_value; break;
		case Physics2DServer::SPACE_PARAM_BODY_TIME_TO_SLEEP: body_time_to_sleep=p_value; break;
		case Physics2DServer::SPACE_PARAM_BODY_ANGULAR_VELOCITY_DAMP_RATIO: body_angular_velocity_damp_ratio=p_value; break;
		case Physics2DServer::SPACE_PARAM_CONSTRAINT_DEFAULT_BIAS: constraint_bias=p_value; break;
	}
}

real_t Space2DSW::get_param(Physics2DServer::SpaceParameter p_param) const {

	switch(p_param) {

		case Physics2DServer::SPACE_PARAM_CONTACT_RECYCLE_RADIUS: return contact_recycle_radius;
		case Physics2DServer::SPACE_PARAM_CONTACT_MAX_SEPARATION: return contact_max_separation;
		case Physics2DServer::SPACE_PARAM_BODY_MAX_ALLOWED_PENETRATION: return contact_max_allowed_penetration;
		case Physics2DServer::SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_TRESHOLD: return body_linear_velocity_sleep_treshold;
		case Physics2DServer::SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_TRESHOLD: return body_angular_velocity_sleep_treshold;
		case Physics2DServer::SPACE_PARAM_BODY_TIME_TO_SLEEP: return body_time_to_sleep;
		case Physics2DServer::SPACE_PARAM_BODY_ANGULAR_VELOCITY_DAMP_RATIO: return body_angular_velocity_damp_ratio;
		case Physics2DServer::SPACE_PARAM_CONSTRAINT_DEFAULT_BIAS: return constraint_bias;
	}
	return 0;
}

void Space2DSW::lock() {

	locked=true;
}

void Space2DSW::unlock() {

	locked=false;
}

bool Space2DSW::is_locked() const {

	return locked;
}

Physics2DDirectSpaceStateSW *Space2DSW::get_direct_state() {

	return direct_access;
}

Space2DSW::Space2DSW() {


	locked=false;
	contact_recycle_radius=0.01;
	contact_max_separation=0.05;
	contact_max_allowed_penetration= 0.01;

	constraint_bias = 0.01;
	body_linear_velocity_sleep_treshold=0.01;
	body_angular_velocity_sleep_treshold=(8.0 / 180.0 * Math_PI);
	body_time_to_sleep=0.5;
	body_angular_velocity_damp_ratio=15;


	broadphase = BroadPhase2DSW::create_func();
	broadphase->set_pair_callback(_broadphase_pair,this);
	broadphase->set_unpair_callback(_broadphase_unpair,this);
	area=NULL;

	direct_access = memnew( Physics2DDirectSpaceStateSW );
	direct_access->space=this;
}

Space2DSW::~Space2DSW() {

	memdelete(broadphase);
	memdelete( direct_access );
}




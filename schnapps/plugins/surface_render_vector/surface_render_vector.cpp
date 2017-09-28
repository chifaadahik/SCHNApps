/*******************************************************************************
* SCHNApps                                                                     *
* Copyright (C) 2015, IGG Group, ICube, University of Strasbourg, France       *
*                                                                              *
* This library is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU Lesser General Public License as published by the *
* Free Software Foundation; either version 2.1 of the License, or (at your     *
* option) any later version.                                                   *
*                                                                              *
* This library is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License  *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with this library; if not, write to the Free Software Foundation,      *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.           *
*                                                                              *
* Web site: http://cgogn.unistra.fr/                                           *
* Contact information: cgogn@unistra.fr                                        *
*                                                                              *
*******************************************************************************/

#include <surface_render_vector.h>

#include <schnapps/core/view.h>
#include <schnapps/core/camera.h>

namespace schnapps
{

namespace plugin_surface_render_vector
{

MapParameters& Plugin_SurfaceRenderVector::get_parameters(View* view, MapHandlerGen* map)
{
	view->makeCurrent();

	auto& view_param_set = parameter_set_[view];
	if (view_param_set.count(map) == 0)
	{
		MapParameters& p = view_param_set[map];
		p.map_ = static_cast<CMap2Handler*>(map);
		return p;
	}
	else
		return view_param_set[map];
}

bool Plugin_SurfaceRenderVector::enable()
{
	if (get_setting("Auto load position attribute").isValid())
		setting_auto_load_position_attribute_ = get_setting("Auto load position attribute").toString();
	else
		setting_auto_load_position_attribute_ = add_setting("Auto load position attribute", "position").toString();

	dock_tab_ = new SurfaceRenderVector_DockTab(this->schnapps_, this);
	schnapps_->add_plugin_dock_tab(this, dock_tab_, "Surface Render Vector");

	connect(schnapps_, SIGNAL(selected_view_changed(View*, View*)), this, SLOT(update_dock_tab()));
	connect(schnapps_, SIGNAL(selected_map_changed(MapHandlerGen*, MapHandlerGen*)), this, SLOT(update_dock_tab()));

	update_dock_tab();

	return true;
}

void Plugin_SurfaceRenderVector::disable()
{
	schnapps_->remove_plugin_dock_tab(this, dock_tab_);
	delete dock_tab_;

	disconnect(schnapps_, SIGNAL(selected_view_changed(View*, View*)), this, SLOT(update_dock_tab()));
	disconnect(schnapps_, SIGNAL(selected_map_changed(MapHandlerGen*, MapHandlerGen*)), this, SLOT(update_dock_tab()));
}

void Plugin_SurfaceRenderVector::draw_map(View* view, MapHandlerGen* map, const QMatrix4x4& proj, const QMatrix4x4& mv)
{
	if (map->dimension() == 2)
	{
		view->makeCurrent();
		const MapParameters& p = get_parameters(view, map);

		if (p.get_position_vbo())
		{
			for (auto& param : p.get_shader_params())
			{
				param->bind(proj, mv);
				map->draw(cgogn::rendering::POINTS);
				param->release();
			}
		}
	}
}

void Plugin_SurfaceRenderVector::view_linked(View* view)
{
	update_dock_tab();

	connect(view, SIGNAL(map_linked(MapHandlerGen*)), this, SLOT(map_linked(MapHandlerGen*)));
	connect(view, SIGNAL(map_unlinked(MapHandlerGen*)), this, SLOT(map_unlinked(MapHandlerGen*)));
	connect(view, SIGNAL(viewerInitialized()), this, SLOT(viewer_initialized()));

	for (MapHandlerGen* map : view->get_linked_maps()) { add_linked_map(view, map); }
}

void Plugin_SurfaceRenderVector::view_unlinked(View* view)
{
	update_dock_tab();

	disconnect(view, SIGNAL(map_linked(MapHandlerGen*)), this, SLOT(map_linked(MapHandlerGen*)));
	disconnect(view, SIGNAL(map_unlinked(MapHandlerGen*)), this, SLOT(map_unlinked(MapHandlerGen*)));
	disconnect(view, SIGNAL(viewerInitialized()), this, SLOT(viewer_initialized()));

	for (MapHandlerGen* map : view->get_linked_maps()) { remove_linked_map(view, map); }
}

void Plugin_SurfaceRenderVector::map_linked(MapHandlerGen *map)
{
	View* view = static_cast<View*>(sender());
	add_linked_map(view, map);
}

void Plugin_SurfaceRenderVector::add_linked_map(View* view, MapHandlerGen *map)
{
	if (map->dimension() == 2)
	{
		set_position_vbo(view, map, map->get_vbo(setting_auto_load_position_attribute_), false);

		connect(map, SIGNAL(vbo_added(cgogn::rendering::VBO*)), this, SLOT(linked_map_vbo_added(cgogn::rendering::VBO*)), Qt::UniqueConnection);
		connect(map, SIGNAL(vbo_removed(cgogn::rendering::VBO*)), this, SLOT(linked_map_vbo_removed(cgogn::rendering::VBO*)), Qt::UniqueConnection);
		connect(map, SIGNAL(bb_changed()), this, SLOT(linked_map_bb_changed()), Qt::UniqueConnection);

		update_dock_tab();
	}
}

void Plugin_SurfaceRenderVector::map_unlinked(MapHandlerGen *map)
{
	View* view = static_cast<View*>(sender());
	remove_linked_map(view, map);
}

void Plugin_SurfaceRenderVector::remove_linked_map(View* view, MapHandlerGen *map)
{
	if (map->dimension() == 2)
	{
		disconnect(map, SIGNAL(vbo_added(cgogn::rendering::VBO*)), this, SLOT(linked_map_vbo_added(cgogn::rendering::VBO*)));
		disconnect(map, SIGNAL(vbo_removed(cgogn::rendering::VBO*)), this, SLOT(linked_map_vbo_removed(cgogn::rendering::VBO*)));
		disconnect(map, SIGNAL(bb_changed()), this, SLOT(linked_map_bb_changed()));

		update_dock_tab();
	}
}

void Plugin_SurfaceRenderVector::linked_map_vbo_added(cgogn::rendering::VBO* vbo)
{
	MapHandlerGen* map = static_cast<MapHandlerGen*>(sender());

	if (vbo->vector_dimension() == 3)
	{
		const QString vbo_name = QString::fromStdString(vbo->name());
		for (auto& it : parameter_set_)
		{
			std::map<MapHandlerGen*, MapParameters>& view_param_set = it.second;
			if (view_param_set.count(map) > 0ul)
			{
				MapParameters& p = view_param_set[map];
				if (!p.get_position_vbo() && vbo_name == setting_auto_load_position_attribute_)
					set_position_vbo(it.first, map, vbo, false);
			}
		}
	}

	if (map->is_selected_map())
	{
		View* view = schnapps_->get_selected_view();
		if (view)
			dock_tab_->update_map_parameters(map, get_parameters(view, map));
	}
}

void Plugin_SurfaceRenderVector::linked_map_vbo_removed(cgogn::rendering::VBO* vbo)
{
	MapHandlerGen* map = static_cast<MapHandlerGen*>(sender());

	if (vbo->vector_dimension() == 3)
	{
		for (auto& it : parameter_set_)
		{
			std::map<MapHandlerGen*, MapParameters>& view_param_set = it.second;
			if (view_param_set.count(map) > 0ul)
			{
				MapParameters& p = view_param_set[map];
				if (p.get_position_vbo() == vbo)
					p.set_position_vbo(nullptr);
				if (p.get_vector_vbo_index(vbo) != UINT32_MAX)
					p.remove_vector_vbo(vbo);
			}
		}

		for (View* view : map->get_linked_views())
			view->update();
	}

	if (map->is_selected_map())
	{
		View* view = schnapps_->get_selected_view();
		if (view)
			dock_tab_->update_map_parameters(map, get_parameters(view, map));
	}
}

void Plugin_SurfaceRenderVector::linked_map_bb_changed()
{
	MapHandlerGen* map = static_cast<MapHandlerGen*>(sender());

	for (auto& it : parameter_set_)
	{
		std::map<MapHandlerGen*, MapParameters>& view_param_set = it.second;
		if (view_param_set.count(map) > 0)
		{
			MapParameters& p = view_param_set[map];
			for (uint32 i = 0, size = p.vector_scale_factor_list_.size(); i < size; ++i)
				p.set_vector_scale_factor(i, p.vector_scale_factor_list_[i]);
		}
	}

	if (map->is_selected_map())
	{
		View* view = schnapps_->get_selected_view();
		if (view)
			dock_tab_->update_map_parameters(map, get_parameters(view, map));
	}
}

void Plugin_SurfaceRenderVector::viewer_initialized()
{
	View* view = dynamic_cast<View*>(sender());
	if (view && (this->parameter_set_.count(view) > 0))
	{
		auto& view_param_set = parameter_set_[view];
		for (auto& p : view_param_set)
			p.second.initialize_gl();
	}
}

void Plugin_SurfaceRenderVector::update_dock_tab()
{
	MapHandlerGen* map = schnapps_->get_selected_map();
	View* view = schnapps_->get_selected_view();
	if (view->is_linked_to_plugin(this) && map && map->is_linked_to_view(view) && map->dimension() == 2)
	{
		schnapps_->enable_plugin_tab_widgets(this);
		dock_tab_->update_map_parameters(map, get_parameters(view, map));
	}
	else
		schnapps_->disable_plugin_tab_widgets(this);
}

/******************************************************************************/
/*                             PUBLIC INTERFACE                               */
/******************************************************************************/

void Plugin_SurfaceRenderVector::set_position_vbo(View* view, MapHandlerGen* map, cgogn::rendering::VBO* vbo, bool update_dock_tab)
{
	if (view && view->is_linked_to_plugin(this) && map && map->is_linked_to_view(view) && map->dimension() == 2)
	{
		MapParameters& p = get_parameters(view, map);
		p.set_position_vbo(vbo);
		if (update_dock_tab && view->is_selected_view() && map->is_selected_map())
			dock_tab_->update_map_parameters(map, p);
		view->update();
	}
}

void Plugin_SurfaceRenderVector::add_vector_vbo(View* view, MapHandlerGen* map, cgogn::rendering::VBO* vbo, bool update_dock_tab)
{
	if (view && view->is_linked_to_plugin(this) && map && map->is_linked_to_view(view) && map->dimension() == 2)
	{
		MapParameters& p = get_parameters(view, map);
		p.add_vector_vbo(vbo);
		if (update_dock_tab && view->is_selected_view() && map->is_selected_map())
			dock_tab_->update_map_parameters(map, p);
		view->update();
	}
}

void Plugin_SurfaceRenderVector::remove_vector_vbo(View* view, MapHandlerGen* map, cgogn::rendering::VBO* vbo, bool update_dock_tab)
{
	if (view && view->is_linked_to_plugin(this) && map && map->is_linked_to_view(view) && map->dimension() == 2)
	{
		MapParameters& p = get_parameters(view, map);
		p.remove_vector_vbo(vbo);
		if (update_dock_tab && view->is_selected_view() && map->is_selected_map())
			dock_tab_->update_map_parameters(map, p);
		view->update();
	}
}

void Plugin_SurfaceRenderVector::set_vector_scale_factor(View* view, MapHandlerGen* map, cgogn::rendering::VBO* vector_vbo, float32 sf, bool update_dock_tab)
{
	if (view && view->is_linked_to_plugin(this) && map && map->is_linked_to_view(view) && map->dimension() == 2)
	{
		MapParameters& p = get_parameters(view, map);
		const uint32 index = p.get_vector_vbo_index(vector_vbo);
		if (index != UINT32_MAX)
		{
			p.set_vector_scale_factor(index, sf);
			if (update_dock_tab && view->is_selected_view() && map->is_selected_map())
				dock_tab_->update_map_parameters(map, p);
			view->update();
		}
	}
}

void Plugin_SurfaceRenderVector::set_vector_color(View* view, MapHandlerGen* map, cgogn::rendering::VBO* vector_vbo, const QColor& color, bool update_dock_tab)
{
	if (view && view->is_linked_to_plugin(this) && map && map->is_linked_to_view(view) && map->dimension() == 2)
	{
		MapParameters& p = get_parameters(view, map);
		const uint32 index = p.get_vector_vbo_index(vector_vbo);
		if (index != UINT32_MAX)
		{
			p.set_vector_color(index, color);
			if (update_dock_tab && view->is_selected_view() && map->is_selected_map())
				dock_tab_->update_map_parameters(map, p);
			view->update();
		}
	}
}

} // namespace plugin_surface_render_vector

} // namespace schnapps

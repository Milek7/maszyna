#include "stdafx.h"
#include "widgets/map.h"
#include "imgui/imgui.h"
#include "Logs.h"
#include "Train.h"
#include "Camera.h"
#include "simulation.h"
#include "Driver.h"

ui::map_panel::map_panel() : ui_panel(LOC_STR(ui_map), false)
{
	size_min = { 200, 200 };
	size_max = { fb_size, fb_size };
	window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	gl::shader vert("map.vert");
	gl::shader frag("map.frag");
	gl::program *prog = new gl::program({vert, frag});
	m_shader = std::unique_ptr<gl::program>(prog);

	m_tex = std::make_unique<opengl_texture>();
	m_tex->alloc_rendertarget(GL_RGB8, GL_RGB, fb_size, fb_size);

	m_fb = std::make_unique<gl::framebuffer>();
	m_fb->attach(*m_tex, GL_COLOR_ATTACHMENT0);

	m_fb->setup_drawing(1);

	if (!m_fb->is_complete())
	{
		ErrorLog("map framebuffer incomplete");
		return;
	}

	if (Global.iMultisampling)
	{
		m_msaa_rb = std::make_unique<gl::renderbuffer>();
		m_msaa_rb->alloc(GL_RGB8, fb_size, fb_size, 1 << Global.iMultisampling);

		m_msaa_fb = std::make_unique<gl::framebuffer>();
		m_msaa_fb->attach(*m_msaa_rb, GL_COLOR_ATTACHMENT0);

		m_msaa_fb->setup_drawing(1);

		if (!m_msaa_fb->is_complete())
		{
			ErrorLog("map multisampling framebuffer incomplete");
			return;
		}
	}

	scene_ubo = std::make_unique<gl::ubo>(sizeof(gl::scene_ubs), 0);

	init_done = true;
}

float ui::map_panel::get_vehicle_rotation()
{
    const TDynamicObject *vehicle = simulation::Train->Dynamic();
    glm::vec3 front = glm::dvec3(vehicle->VectorFront()) * (vehicle->DirectionGet() > 0 ? 1.0 : -1.0);
    glm::vec2 north_ptr(0.0f, 1.0f);
	return glm::atan(front.z, front.x) - glm::atan(north_ptr.y, north_ptr.x);
}

void ui::map_panel::render_map_texture(glm::mat4 transform, glm::vec2 surface_size) {
	cFrustum frustum;
	frustum.calculate(transform, glm::mat4());

	m_section_handles.clear();
	m_switch_handles.clear();

	for (int row = 0; row < scene::EU07_REGIONSIDESECTIONCOUNT; row++)
	{
		for (int column = 0; column < scene::EU07_REGIONSIDESECTIONCOUNT; column++)
		{
			scene::basic_section *section = simulation::Region->get_section(row * scene::EU07_REGIONSIDESECTIONCOUNT + column);
			if (section && frustum.sphere_inside(section->area().center, section->area().radius) > 0.f)
			{
				const gfx::geometrybank_handle handle = section->get_map_geometry();
				if (handle != null_handle) {
					m_section_handles.push_back(handle);
					section->get_map_active_switches(m_switch_handles);
				}
			}
		}
	}

	glDisable(GL_DEPTH_TEST);
	if (Global.iMultisampling)
	{
		m_msaa_fb->clear(GL_COLOR_BUFFER_BIT);
		m_msaa_fb->bind();
	}
	else
	{
		m_fb->clear(GL_COLOR_BUFFER_BIT);
		m_fb->bind();
	}

	m_shader->bind();
	glLineWidth(1.5f);
	glViewport(0, 0, surface_size.x, surface_size.y);

	scene_ubs.projection = transform;
	scene_ubs.time = 0.3f; // color is stuffed in time variable
	scene_ubo->update(scene_ubs);
	scene_ubo->bind_uniform();

	GfxRenderer.Draw_Geometry(m_section_handles.begin(), m_section_handles.end());

	scene_ubs.time = 0.6f; // color is stuffed in time variable
	scene_ubo->update(scene_ubs);
	GfxRenderer.Draw_Geometry(m_switch_handles.begin(), m_switch_handles.end());

	if (Global.iMultisampling)
		m_fb->blit_from(m_msaa_fb.get(), surface_size.x, surface_size.y, GL_COLOR_BUFFER_BIT, GL_COLOR_ATTACHMENT0);

	gl::framebuffer::unbind();
	m_shader->unbind();
}

void ui::map_panel::render_labels(glm::mat4 transform, ImVec2 origin, glm::vec2 surface_size)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 1.0f, 0.7f, 1.0f));
	for (TDynamicObject *vehicle : simulation::Vehicles.sequence()) {
		if (vehicle->Prev() || !vehicle->Mechanik)
			continue;
		if (vehicle->Mechanik->TrainName().empty())
			continue;

		glm::vec4 ndc_pos = transform * glm::vec4(glm::vec3(vehicle->GetPosition()), 1.0f);
		if (glm::abs(ndc_pos.x) > 1.0f || glm::abs(ndc_pos.z) > 1.0f)
			continue;

		glm::vec2 gui_pos = (glm::vec2(ndc_pos.x, -ndc_pos.z) / 2.0f + 0.5f) * glm::vec2(surface_size.x, surface_size.y);

		TDynamicObject *veh = vehicle;

		const char *desc = vehicle->Mechanik->TrainName().c_str();
		ImVec2 textsize = ImGui::CalcTextSize(desc);
		ImGui::SetCursorPos(ImVec2(origin.x + gui_pos.x - textsize.x / 2.0f,
		                           origin.y + gui_pos.y - textsize.y / 2.0f));
		ImGui::TextUnformatted(desc);
	}
	ImGui::PopStyleColor();
}

void ui::map_panel::render_contents()
{
	if (!init_done)
		return;

	float prev_zoom = zoom;

	if (ImGui::Button("-"))
		zoom /= 2;
	ImGui::SameLine();

	if (ImGui::Button("+"))
		zoom *= 2;
	ImGui::SameLine();

	float x = zoom / prev_zoom;
	translate *= x;

	glm::mat4 transform;
	transform[0][0] = -1.0f;

	static enum {
		MODE_MANUAL = 0,
		MODE_CAMERA,
		MODE_VEHICLE
	} mode = MODE_MANUAL;

	ImGui::RadioButton("manual", (int*)&mode, 0); ImGui::SameLine();
	ImGui::RadioButton("cam", (int*)&mode, 1); ImGui::SameLine();
	ImGui::RadioButton("vehicle", (int*)&mode, 2);

	ImVec2 surface_size_im = ImGui::GetContentRegionAvail();
	glm::vec2 surface_size(surface_size_im.x, surface_size_im.y);

	float aspect = surface_size.y / surface_size.x;

	if (aspect > 1.0f / aspect)
		transform = glm::scale(transform, glm::vec3(aspect, 1.0f, 1.0f));
	else
		transform = glm::scale(transform, glm::vec3(1.0f, 1.0f, 1.0f / aspect));

	if (mode == MODE_VEHICLE && simulation::Train)
	{
		float rotation = get_vehicle_rotation();

		transform = glm::rotate(transform, rotation, glm::vec3(0.0f, 1.0f, 0.0f));

		glm::dvec3 position = simulation::Train->Dynamic()->GetPosition();
		translate = glm::vec2(position.x, position.z) * -zoom;
	}
	if (mode == MODE_CAMERA)
	{
		float initial_rotation;
		if (!FreeFlyModeFlag)
			initial_rotation = get_vehicle_rotation();
		else
			initial_rotation = glm::pi<float>();

		float rotation = initial_rotation - Global.pCamera.Angle.y;

		transform = glm::rotate(transform, rotation, glm::vec3(0.0f, 1.0f, 0.0f));

		glm::dvec3 position = Global.pCamera.Pos;
		translate = glm::vec2(position.x, position.z) * -zoom;
	}

	transform = glm::translate(transform, glm::vec3(translate.x, 0.0f, translate.y));
	transform = glm::scale(transform, glm::vec3(zoom));

	render_map_texture(transform, surface_size);

	ImVec2 window_origin = ImGui::GetCursorPos();
	ImVec2 screen_origin = ImGui::GetCursorScreenPos();
	ImGui::ImageButton(reinterpret_cast<void*>(m_tex->id), surface_size_im, ImVec2(0, surface_size.y / fb_size), ImVec2(surface_size.x / fb_size, 0), 0);

	if (ImGui::IsItemHovered())
	{
		if (mode == 0 && ImGui::IsMouseDragging(0)) {
			ImVec2 delta_im = ImGui::GetMouseDragDelta();
			ImGui::ResetMouseDragDelta();

			glm::vec2 delta(delta_im.x, delta_im.y);
			delta.x /= surface_size.x;
			delta.y /= surface_size.y;

			translate -= delta * 2.0f;
		}
		if (ImGui::IsMouseClicked(1)) {
			ImVec2 screen_pos = ImGui::GetMousePos();
			glm::vec2 surface_pos(screen_pos.x - screen_origin.x, screen_pos.y - screen_origin.y);
			glm::vec2 ndc_pos = surface_pos / surface_size * 2.0f - 1.0f;
			glm::vec3 world_pos = glm::inverse(transform) * glm::vec4(ndc_pos.x, 0.0f, -ndc_pos.y, 1.0f);

			std::vector<TEventLauncher *> launchers = simulation::Events.find_eventlaunchers(glm::vec2(world_pos.x, world_pos.z), 15.0f);

			for (auto launcher : launchers) {
				command_relay relay;
				if (!Global.shiftState && launcher->Event1)
					relay.post(user_command::queueevent, (double)simulation::Events.GetEventId(launcher->Event1), 0.0, GLFW_PRESS, 0);
				else if (launcher->Event2)
					relay.post(user_command::queueevent, (double)simulation::Events.GetEventId(launcher->Event2), 0.0, GLFW_PRESS, 0);
			}
		}
	}

	render_labels(transform, window_origin, surface_size);
}
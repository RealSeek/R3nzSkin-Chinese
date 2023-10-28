#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "CheatManager.hpp"
#include "GUI.hpp"

#include <ranges>

#include "Memory.hpp"
#include "SkinDatabase.hpp"
#include "Utils.hpp"
#include "fnv_hash.hpp"
#include "imgui/imgui.h"

// UTF8编码支持
#define _S(_LITERAL)    (const char*)u8##_LITERAL

inline static void footer() noexcept
{
	using namespace std::string_literals;
	static const auto buildText{ "Update: "s + __DATE__ + " - " + __TIME__ };
	ImGui::Separator();
	ImGui::textUnformattedCentered(buildText.c_str());
	ImGui::textUnformattedCentered("RealSeek");
}

static void changeTurretSkin(const std::int32_t skinId, const std::int32_t team) noexcept
{
	if (skinId == -1)
		return;

	const auto turrets{ cheatManager.memory->turretList };
	const auto playerTeam{ cheatManager.memory->localPlayer->get_team() };

	for (auto i{ 0u }; i < turrets->length; ++i) {
		if (const auto turret{ turrets->list[i] }; turret->get_team() == team) {
			if (playerTeam == team) {
				turret->get_character_data_stack()->base_skin.skin = skinId * 2;
				turret->get_character_data_stack()->update(true);
			} else {
				turret->get_character_data_stack()->base_skin.skin = skinId * 2 + 1;
				turret->get_character_data_stack()->update(true);
			}
		}
	}
}

void GUI::render() noexcept
{
	const auto player{ cheatManager.memory->localPlayer };
	const auto heroes{ cheatManager.memory->heroList };
	static const auto my_team{ player ? player->get_team() : 100 };
	static int gear{ player ? player->get_character_data_stack()->base_skin.gear : 0 };

	static const auto vector_getter_skin = [](void* vec, const std::int32_t idx, const char** out_text) noexcept {
		const auto& vector{ *static_cast<std::vector<SkinDatabase::skin_info>*>(vec) };
		if (idx < 0 || idx > static_cast<std::int32_t>(vector.size())) return false;
		*out_text = idx == 0 ? "Default" : vector.at(idx - 1).skin_name.c_str();
		return true;
	};

	static const auto vector_getter_ward_skin = [](void* vec, const std::int32_t idx, const char** out_text) noexcept {
		const auto& vector{ *static_cast<std::vector<std::pair<std::int32_t, const char*>>*>(vec) };
		if (idx < 0 || idx > static_cast<std::int32_t>(vector.size())) return false;
		*out_text = idx == 0 ? "Default" : vector.at(idx - 1).second;
		return true;
	};

	static auto vector_getter_gear = [](void* vec, const std::int32_t idx, const char** out_text) noexcept {
		const auto& vector{ *static_cast<std::vector<const char*>*>(vec) };
		if (idx < 0 || idx > static_cast<std::int32_t>(vector.size())) return false;
		*out_text = vector[idx];
		return true;
	};

	static auto vector_getter_default = [](void* vec, const std::int32_t idx, const char** out_text) noexcept {
		const auto& vector{ *static_cast<std::vector<const char*>*>(vec) };
		if (idx < 0 || idx > static_cast<std::int32_t>(vector.size())) return false;
		*out_text = idx == 0 ? "Default" : vector.at(idx - 1);
		return true;
	};

	ImGui::Begin("R3nzSkin", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize);
	{
		ImGui::rainbowText();
		if (ImGui::BeginTabBar("TabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_NoTooltip)) {
			if (player) {
				if (ImGui::BeginTabItem(_S("本地玩家"))) {
					auto& values{ cheatManager.database->champions_skins[fnv::hash_runtime(player->get_character_data_stack()->base_skin.model.str)] };
					ImGui::Text(_S("玩家皮肤设置:"));

					if (ImGui::Combo(_S("当前皮肤"), &cheatManager.config->current_combo_skin_index, vector_getter_skin, static_cast<void*>(&values), values.size() + 1))
						if (cheatManager.config->current_combo_skin_index > 0)
							player->change_skin(values[cheatManager.config->current_combo_skin_index - 1].model_name, values[cheatManager.config->current_combo_skin_index - 1].skin_id);
					
					const auto playerHash{ fnv::hash_runtime(player->get_character_data_stack()->base_skin.model.str) };
					if (const auto it{ std::ranges::find_if(cheatManager.database->specialSkins,
					[& skin = player->get_character_data_stack()->base_skin.skin, &ph = playerHash](const SkinDatabase::specialSkin& x) noexcept -> bool
						{
						   return x.champHash == ph && (x.skinIdStart <= skin && x.skinIdEnd >= skin);
						})
					}; it != cheatManager.database->specialSkins.end())
					{
						const auto stack{ player->get_character_data_stack() };
						gear = stack->base_skin.gear;

						if (ImGui::Combo(_S("皮肤档位"), &gear, vector_getter_gear, static_cast<void*>(&it->gears), it->gears.size())) {
							player->get_character_data_stack()->base_skin.gear = static_cast<std::int8_t>(gear);
							player->get_character_data_stack()->update(true);
						}
						ImGui::Separator();
					}

					if (ImGui::Combo(_S("守卫眼皮肤"), &cheatManager.config->current_combo_ward_index, vector_getter_ward_skin, static_cast<void*>(&cheatManager.database->wards_skins), cheatManager.database->wards_skins.size() + 1))
						cheatManager.config->current_ward_skin_index = cheatManager.config->current_combo_ward_index == 0 ? -1 : cheatManager.database->wards_skins.at(cheatManager.config->current_combo_ward_index - 1).first;
					footer();
					ImGui::EndTabItem();
				}
			}

			if (ImGui::BeginTabItem(_S("其他玩家"))) {
				ImGui::Text(_S("其他召唤师皮肤设置:"));
				std::int32_t last_team{ 0 };
				for (auto i{ 0u }; i < heroes->length; ++i) {
					const auto hero{ heroes->list[i] };

					if (hero == player)
						continue;

					const auto champion_name_hash{ fnv::hash_runtime(hero->get_character_data_stack()->base_skin.model.str) };
					if (champion_name_hash == FNV("PracticeTool_TargetDummy"))
						continue;

					const auto hero_team{ hero->get_team() };
					const auto is_enemy{ hero_team != my_team };

					if (last_team == 0 || hero_team != last_team) {
						if (last_team != 0)
							ImGui::Separator();
						if (is_enemy)
							ImGui::Text(_S(" 敌方英雄"));
						else
							ImGui::Text(_S(" 所有队友"));
						last_team = hero_team;
					}

					auto& config_array{ is_enemy ? cheatManager.config->current_combo_enemy_skin_index : cheatManager.config->current_combo_ally_skin_index };
					const auto [fst, snd]{ config_array.insert({ champion_name_hash, 0 }) };

					std::snprintf(this->str_buffer, sizeof(this->str_buffer), cheatManager.config->heroName ? "HeroName: [ %s ]##%X" : "PlayerName: [ %s ]##%X", cheatManager.config->heroName ? hero->get_character_data_stack()->base_skin.model.str : hero->get_name()->c_str(), reinterpret_cast<std::uintptr_t>(hero));

					auto& values{ cheatManager.database->champions_skins[champion_name_hash] };
					if (ImGui::Combo(str_buffer, &fst->second, vector_getter_skin, static_cast<void*>(&values), values.size() + 1))
						if (fst->second > 0)
							hero->change_skin(values[fst->second - 1].model_name, values[fst->second - 1].skin_id);
				}
				footer();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem(_S("对局皮肤"))) {
				ImGui::Text("Global Skins Settings:");
				if (ImGui::Combo("Minion Skins:", &cheatManager.config->current_combo_minion_index, vector_getter_default, static_cast<void*>(&cheatManager.database->minions_skins), cheatManager.database->minions_skins.size() + 1))
					cheatManager.config->current_minion_skin_index = cheatManager.config->current_combo_minion_index - 1;
				ImGui::Separator();
				if (ImGui::Combo("Order Turret Skins:", &cheatManager.config->current_combo_order_turret_index, vector_getter_default, static_cast<void*>(&cheatManager.database->turret_skins), cheatManager.database->turret_skins.size() + 1))
					changeTurretSkin(cheatManager.config->current_combo_order_turret_index - 1, 100);
				if (ImGui::Combo("Chaos Turret Skins:", &cheatManager.config->current_combo_chaos_turret_index, vector_getter_default, static_cast<void*>(&cheatManager.database->turret_skins), cheatManager.database->turret_skins.size() + 1))
					changeTurretSkin(cheatManager.config->current_combo_chaos_turret_index - 1, 200);
				ImGui::Separator();
				ImGui::Text("Jungle Mobs Skins Settings:");
				for (auto& it : cheatManager.database->jungle_mobs_skins) {
					std::snprintf(str_buffer, 256, "Current %s skin", it.name);
					const auto [fst, snd]{ cheatManager.config->current_combo_jungle_mob_skin_index.insert({ it.name_hashes.front(), 0 }) };
					if (ImGui::Combo(str_buffer, &fst->second, vector_getter_default, &it.skins, it.skins.size() + 1))
						for (const auto& hash : it.name_hashes)
							cheatManager.config->current_combo_jungle_mob_skin_index[hash] = fst->second;
				}
				footer();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem(_S("日志"))) {
				cheatManager.logger->draw();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem(_S("杂项"))) {
				ImGui::hotkey(_S("菜单快捷键"), cheatManager.config->menuKey);
				ImGui::Checkbox(cheatManager.config->heroName ? _S("以英雄名字为准") : _S("以召唤师名称为准"), &cheatManager.config->heroName);
				ImGui::Checkbox(_S("彩虹文本"), &cheatManager.config->rainbowText);
				ImGui::Checkbox(_S("快捷键换肤"), &cheatManager.config->quickSkinChange);
				ImGui::hoverInfo(_S("它允许您在不打开菜单的情况下修改你的皮肤"));

				if (cheatManager.config->quickSkinChange) {
					ImGui::Separator();
					ImGui::hotkey(_S("上一个皮肤按键"), cheatManager.config->previousSkinKey);
					ImGui::hotkey(_S("下一个皮肤按键"), cheatManager.config->nextSkinKey);
					ImGui::Separator();
				}

				if (player)
					ImGui::InputText(_S("修改游戏内的召唤师名称"), player->get_name());

				if (ImGui::Button(_S("去掉所有人的皮肤(除了你自己)"))) {
					for (auto& val : cheatManager.config->current_combo_enemy_skin_index | std::views::values)
						val = 1;

					for (auto& val : cheatManager.config->current_combo_ally_skin_index | std::views::values)
						val = 1;

					for (auto i{ 0u }; i < heroes->length; ++i) {
						if (const auto hero{ heroes->list[i] }; hero != player)
							hero->change_skin(hero->get_character_data_stack()->base_skin.model.str, 0);
					}
				} ImGui::hoverInfo(_S("字面意思"));

				if (ImGui::Button(_S("随机皮肤"))) {
					for (auto i{ 0u }; i < heroes->length; ++i) {
						const auto hero{ heroes->list[i] };
						const auto championHash{ fnv::hash_runtime(hero->get_character_data_stack()->base_skin.model.str) };
						
						if (championHash == FNV("PracticeTool_TargetDummy"))
							continue;
						
						const auto skinCount{ cheatManager.database->champions_skins[championHash].size() };
						auto& skinDatabase{ cheatManager.database->champions_skins[championHash] };
						auto& config{ (hero->get_team() != my_team) ? cheatManager.config->current_combo_enemy_skin_index : cheatManager.config->current_combo_ally_skin_index };

						if (hero == player) {
							cheatManager.config->current_combo_skin_index = random(1ull, skinCount);
							hero->change_skin(skinDatabase[cheatManager.config->current_combo_skin_index - 1].model_name, skinDatabase[cheatManager.config->current_combo_skin_index - 1].skin_id);
						} else {
							auto& data{ config[championHash] };
							data = random(1ull, skinCount);
							hero->change_skin(skinDatabase[data - 1].model_name, skinDatabase[data - 1].skin_id);
						}
					}
				} ImGui::hoverInfo(_S("给这场对局的所有英雄随机修改一个皮肤"));

				ImGui::SliderFloat(_S("字体大小"), &cheatManager.config->fontScale, 1.0f, 2.0f, "%.3f");
				if (ImGui::GetIO().FontGlobalScale != cheatManager.config->fontScale) {
					ImGui::GetIO().FontGlobalScale = cheatManager.config->fontScale;
				} ImGui::hoverInfo(_S("改变菜单字体的大小"));
				
				if (ImGui::Button(_S("退出游戏")))
					cheatManager.hooks->uninstall();
				ImGui::hoverInfo(_S("你将会返回到游戏大厅内"));
				ImGui::Text("FPS: %.0f FPS", ImGui::GetIO().Framerate);
				footer();
				ImGui::EndTabItem();
			}
		}
	}
	ImGui::End();
}

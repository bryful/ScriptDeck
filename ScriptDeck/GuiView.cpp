#include "GuiView.h"
#include "HomeDeck.h"
#include "Utf8Path.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"
#include <sstream>
#include <algorithm>
#include <utility>
#include <stdexcept>

namespace {
ImVec4 ColorVector(const DeckColor& color) { return ImVec4(color[0],color[1],color[2],color[3]); }
ImU32 ColorPacked(const DeckColor& color) { return ImGui::ColorConvertFloat4ToU32(ColorVector(color)); }
DeckColor ColorArray(const ImVec4& color) { return {color.x,color.y,color.z,color.w}; }
bool EditColor(const char* label, std::optional<DeckColor>& color, const ImVec4& initial)
{
    ImGui::PushID(label);
    bool enabled = color.has_value();
    bool changed = ImGui::Checkbox(label, &enabled);
    if (changed) { if (enabled) color = ColorArray(initial); else color.reset(); }
    if (color) {
        ImGui::SetNextItemWidth(-1);
        changed |= ImGui::ColorEdit4("##rgba", color->data(), ImGuiColorEditFlags_AlphaPreviewHalf);
    }
    ImGui::PopID();
    return changed;
}
}
GuiView::GuiView(const LaunchOptions& options, GuiServices services)
    : path_(options.stackFile), services_(std::move(services)),
      magic_(options.mode == LaunchMode::Magic)
{
    const bool useHome=path_.empty();
    if(useHome)path_=EnsureHomeDeck();
    path_=std::filesystem::absolute(path_).lexically_normal();
    if(!std::filesystem::exists(path_) && magic_ && !useHome) {
        stack_.CreateNew(PathToUtf8(path_.stem()));
        if(!stack_.Save(path_))throw std::runtime_error("Cannot create deck: "+stack_.LastError());
    } else if(!stack_.Load(path_)) {
        throw std::runtime_error("Cannot load deck: "+stack_.LastError());
    }
}
bool GuiView::NavigateCard(const std::string& action, const nlohmann::json& target)
{
    if(pendingDeck_)throw std::runtime_error("A deck change is already pending.");
    const auto count=stack_.cards.size();
    if(!count)return false;
    auto current=std::find_if(stack_.cards.begin(),stack_.cards.end(),[&](const Card& c){return c.id==stack_.currentCardId;});
    auto index=current==stack_.cards.end()?std::size_t(0):static_cast<std::size_t>(current-stack_.cards.begin());
    if(action=="next") {if(index+1>=count)return false;++index;}
    else if(action=="previous") {if(!index)return false;--index;}
    else if(action=="top")index=0;
    else if(action=="end")index=count-1;
    else if(action=="index") {
        if(!target.is_number_integer() || target.get<double>()<0 || target.get<double>()>=static_cast<double>(count))
            throw std::invalid_argument("Card index is out of range (zero based).");
        index=target.get<std::size_t>();
    } else if(action=="nameOrId") {
        if(!target.is_string())throw std::invalid_argument("Card name or ID must be a string.");
        const auto key=target.get<std::string>();
        const auto* byId=stack_.FindCard(key);
        if(byId)index=static_cast<std::size_t>(byId-stack_.cards.data());
        else {
            std::optional<std::size_t> found;
            for(std::size_t i=0;i<count;++i)if(stack_.cards[i].name==key) {
                if(found)throw std::invalid_argument("Card name is ambiguous; use its ID.");
                found=i;
            }
            if(!found)throw std::invalid_argument("Card was not found.");
            index=*found;
        }
    } else throw std::invalid_argument("Unknown card navigation action.");
    if(stack_.currentCardId==stack_.cards[index].id)return false;
    stack_.currentCardId=stack_.cards[index].id;
    selected_.clear();buttonEvents_.clear();fileDropEvents_.clear();dirty_ = true;
    if(ImGui::GetCurrentContext())ImGui::ClearActiveID();
    return true;
}
void GuiView::RequestNewDeck()
{
    if(pendingDeck_)throw std::runtime_error("A deck change is already pending.");
    if(dirty_&&path_.empty())throw std::runtime_error("Save the current deck before creating a new deck.");
    Stack candidate;candidate.CreateNew();
    pendingDeck_=PendingDeck{std::move(candidate),{},!path_.empty(),true};
}
void GuiView::RequestHome(bool saveCurrent)
{
    RequestDeckChange(EnsureHomeDeck(),saveCurrent);
}
void GuiView::RequestDeckChange(const std::filesystem::path& path, bool saveCurrent)
{
    if(pendingDeck_)throw std::runtime_error("A deck change is already pending.");
    if(path.empty())throw std::invalid_argument("Deck path is empty.");
    const auto destination=std::filesystem::absolute(path).lexically_normal();
    Stack candidate;
    if(!candidate.Load(destination))throw std::runtime_error("Cannot load deck: "+candidate.LastError());
    if(saveCurrent && path_.empty())throw std::runtime_error("Current deck has no save path; save it first or pass false.");
    pendingDeck_=PendingDeck{std::move(candidate),destination,saveCurrent};
}
void GuiView::RequestOpenDeck(const std::filesystem::path& path)
{
    const auto source=path.empty()?path_:path;
    if(source.empty())throw std::runtime_error("Current deck has no file path.");
    RequestDeckChange(source,false);
}
void GuiView::SaveDeck(const std::filesystem::path& path)
{
    if(pendingDeck_)throw std::runtime_error("A deck change is already pending.");
    const auto destination=path.empty()?path_:path;
    if(destination.empty())throw std::runtime_error("Current deck has no save path; use saveAsDeck().");
    const auto absolute=std::filesystem::absolute(destination).lexically_normal();
    if(!stack_.Save(absolute))throw std::runtime_error("Cannot save deck: "+stack_.LastError());
    path_=absolute;dirty_=false;status_.clear();
}
bool GuiView::ApplyPendingDeckChange()
{
    if(!pendingDeck_)return false;
    auto request=std::move(*pendingDeck_);pendingDeck_.reset();
    if(request.saveCurrent) {
        if(!stack_.Save(path_))throw std::runtime_error("Cannot save current deck: "+stack_.LastError());
        // Same-path reload must use the version just saved, rather than its previous contents.
        if(!request.path.empty() && std::filesystem::equivalent(path_,request.path) && !request.data.Load(request.path))
            throw std::runtime_error("Cannot reload saved deck: "+request.data.LastError());
    }
    EndScriptEditor();
    stack_=std::move(request.data);path_=std::move(request.path);
    ++scriptGeneration_;buttonEvents_.clear();fileDropEvents_.clear();selected_.clear();status_.clear();dirty_=false;
    if(request.isNew){dirty_=true;SetMagic(true);}
    drawnCardId_.clear();
    scriptConsoleOpen_=false;consoleCommands_.clear();
    if(ImGui::GetCurrentContext())ImGui::ClearActiveID();
    return true;
}
std::string GuiView::WindowTitle() const
{
    const std::string name = stack_.name.empty() ? "ScriptDeck" : stack_.name;
    if (!magic_) return name;
    return name + " - Magic" + (dirty_ ? " *" : "");
}
std::vector<ButtonEvent> GuiView::TakeButtonEvents()
{
    std::vector<ButtonEvent> events; events.swap(buttonEvents_); return events;
}
void GuiView::PushButton(Card& card, const std::string& buttonId)
{
    const auto* button = card.FindObject(buttonId);
    if (!button || button->type != ObjectType::Button || !button->visible || !button->enabled) return;
    buttonEvents_.push_back({card.id, button->id, "mouseUp", false, "", scriptGeneration_});

}
void GuiView::ChangeChecked(Card& card, const std::string& objectId, bool value)
{
    std::vector<std::pair<std::string,bool>> before;
    for(const auto& object:card.objects)if(object.type==ObjectType::Checkbox||object.type==ObjectType::RadioButton)before.emplace_back(object.id,object.checked);
    if(!card.SetChecked(objectId,value))return;
    dirty_ = true;
    // 同一グループの解除イベントを先に、操作対象を最後に配送する。
    for(const auto& state:before)if(state.first!=objectId) {
        const auto* object=card.FindObject(state.first);
        if(object->checked!=state.second)buttonEvents_.push_back({card.id,object->id,"change",object->checked,object->group,scriptGeneration_});
    }
    const auto* target=card.FindObject(objectId);
    buttonEvents_.push_back({card.id,objectId,"change",target->checked,target->group,scriptGeneration_});
}
bool GuiView::QueueFileDrop(const std::vector<std::string>& paths, ImVec2 position)
{
    if(scriptConsoleOpen_||!ScriptsEnabled()||paths.empty()||drawnGeneration_!=scriptGeneration_||drawnCardId_!=stack_.currentCardId||
       position.x<cardClipMin_.x||position.y<cardClipMin_.y||position.x>=cardClipMax_.x||position.y>=cardClipMax_.y||
       position.x<cardOrigin_.x||position.y<cardOrigin_.y||position.x>=cardOrigin_.x+cardExtent_.x||position.y>=cardOrigin_.y+cardExtent_.y)return false;
    fileDropEvents_.push_back({drawnCardId_,paths,position.x-cardOrigin_.x,position.y-cardOrigin_.y,scriptGeneration_});return true;
}
std::vector<FileDropEvent> GuiView::TakeFileDropEvents()
{
    std::vector<FileDropEvent> result;result.swap(fileDropEvents_);return result;
}
void GuiView::SetConsoleMode(bool enabled)
{
    if (!services_.setConsoleMode) throw std::runtime_error("Console mode service is unavailable.");
    services_.setConsoleMode(enabled);
}
void GuiView::SetMagic(bool enabled)
{
    if (magic_ == enabled) return;
    EndScriptEditor();
    magic_ = enabled;
    scriptConsoleOpen_ = false;consoleCommands_.clear();
    test_ = false;
    selected_.clear();
    // デッキ・カード・フィールド・スクリプトと未保存状態は維持する。
    // モード切り替えだけでは保存や変更破棄をしない。
}
void GuiView::SetCardSize(int width, int height)
{
    if (width < 1 || height < 1 || width > 8192 || height > 8192)
        throw std::runtime_error("Card size must be between 1 and 8192 pixels.");
    if (stack_.width != width || stack_.height != height) {
        stack_.width = width; stack_.height = height;
        dirty_ = true;
    }
}
Card* GuiView::Current() { return stack_.FindCard(stack_.currentCardId); }
bool GuiView::CanClose()
{
    if(!dirty_)return true;
    if(magic_)return services_.discardChanges();
    try {
        auto destination=path_;
        if(destination.empty()) {
            if(!services_.chooseFile)throw std::runtime_error("Current deck has no save path.");
            destination=services_.chooseFile(true);
            if(destination.empty())return false;
        }
        SaveDeck(destination);
        return true;
    } catch(const std::exception& error) {
        status_="自動保存エラー: "+std::string(error.what());
        if(services_.reportSaveError)services_.reportSaveError(status_);
        return false;
    }
}
bool GuiView::Save(bool saveAs)
{
    auto destination = path_;
    if (saveAs || destination.empty()) destination = services_.chooseFile(true);
    if (destination.empty()) return false;
    if (!stack_.Save(destination)) { status_ = "保存エラー: " + stack_.LastError(); return false; }
    path_ = destination; dirty_ = false; status_.clear();
    return true;
}
void GuiView::Open()
{
    const auto source = services_.chooseFile(false);
    if (source.empty()) return;
    Stack candidate;
    if (!candidate.Load(source)) { status_ = "読み込みエラー: " + candidate.LastError(); return; }
    if (!CanClose()) return;
    pendingDeck_.reset();
    ++scriptGeneration_;fileDropEvents_.clear();buttonEvents_.clear();
    stack_ = std::move(candidate); path_ = source; selected_.clear(); dirty_ = false;
    status_ = "開きました: " + PathToUtf8(path_);
}
void GuiView::New()
{
    if (!CanClose()) return;
    pendingDeck_.reset();
    ++scriptGeneration_;fileDropEvents_.clear();buttonEvents_.clear();
    stack_.CreateNew(); path_.clear(); selected_.clear(); dirty_ = true; status_.clear();
}
void GuiView::Toolbar()
{
    if (ImGui::Button("開く")) Open();
    if (magic_) {
        ImGui::SameLine(); if (ImGui::Button("新規")) New();
        ImGui::SameLine(); if(ImGui::Button("Homeを開く")) {
            try {
                const auto home=EnsureHomeDeck();
                if(CanClose())RequestDeckChange(home,false);
            } catch(const std::exception& error) {
                status_="Home読み込みエラー: "+std::string(error.what());
                if(services_.reportSaveError)services_.reportSaveError(status_);
            }
        }
        ImGui::SameLine(); if (ImGui::Button("保存")) Save(false);
        ImGui::SameLine(); if (ImGui::Button("別名保存")) Save(true);
        ImGui::SameLine(); ImGui::Checkbox("実行プレビュー", &test_);
        ImGui::SameLine(); if (ImGui::Button("Playerモードへ")) SetMagic(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("< 前")) {
        for (std::size_t i = 1; i < stack_.cards.size(); ++i)
            if (stack_.cards[i].id == stack_.currentCardId) {
                stack_.currentCardId = stack_.cards[i-1].id; selected_.clear(); dirty_ = true; break;
            }
    }
    ImGui::SameLine();
    if (ImGui::Button("次 >")) {
        for (std::size_t i = 0; i + 1 < stack_.cards.size(); ++i)
            if (stack_.cards[i].id == stack_.currentCardId) {
                stack_.currentCardId = stack_.cards[i+1].id; selected_.clear(); dirty_ = true; break;
            }
    }
    ImGui::Text("%s  |  %s", stack_.name.c_str(), magic_ ? "Magic" : "Player");
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
}
void GuiView::Editor()
{
    auto orderControls = [&](const char* scope, auto& items, const std::string& selectedId) {
        ImGui::PushID(scope);
        auto selected = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.id == selectedId; });
        const bool canMoveUp = selected != items.end() && selected != items.begin();
        ImGui::BeginDisabled(!canMoveUp);
        if (ImGui::Button("上へ") && canMoveUp) {
            std::iter_swap(selected, selected - 1); dirty_ = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        selected = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.id == selectedId; });
        const bool canMoveDown = selected != items.end() && selected + 1 != items.end();
        ImGui::BeginDisabled(!canMoveDown);
        if (ImGui::Button("下へ") && canMoveDown) {
            std::iter_swap(selected, selected + 1); dirty_ = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    };

    if (ImGui::InputText("デッキ名", &stack_.name)) dirty_ = true;
    int size[2] = {stack_.width, stack_.height};
    if (ImGui::InputInt2("画面サイズ", size)) {
        stack_.width = std::clamp(size[0], 64, 8192); stack_.height = std::clamp(size[1], 64, 8192); dirty_ = true;
    }
    int placement = (stack_.startupPlacement == "previous" || stack_.startupPlacement == "previous_default") ? 1 :
                    stack_.startupPlacement == "previous_center" ? 2 : stack_.startupPlacement == "center" ? 3 : 0;
    ImGui::TextUnformatted("Player起動位置");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##startupPlacement", &placement, "デフォルト\0前回実行時の位置・サイズ（デフォルト）\0前回実行時の位置・サイズ（画面中央）\0画面中央\0")) {
        const char* values[] = {"default", "previous_default", "previous_center", "center"};
        stack_.startupPlacement = values[placement]; dirty_ = true;
    }
    ImGui::TextWrapped("前回の記録がない場合、括弧内の位置で起動します。設定は保存後、次のPlayer起動時に適用されます。");
    ImGui::SeparatorText("カード");
    for (const auto& c : stack_.cards) {
        ImGui::PushID(c.id.c_str());
        if (ImGui::Selectable((c.name + "##" + c.id).c_str(), c.id == stack_.currentCardId)) {
            stack_.currentCardId = c.id; selected_.clear(); dirty_ = true;
        }
        ImGui::PopID();
    }
    orderControls("cardOrder", stack_.cards, stack_.currentCardId);
    if (ImGui::Button("カード追加")) {
        stack_.currentCardId = stack_.AddCard("新しいカード").id; selected_.clear(); dirty_ = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(stack_.cards.size() <= 1);
    if (ImGui::Button("カード削除")) {
        const auto id = stack_.currentCardId;
        stack_.cards.erase(std::remove_if(stack_.cards.begin(), stack_.cards.end(),
            [&](const Card& c) { return c.id == id; }), stack_.cards.end());
        stack_.currentCardId = stack_.cards.front().id; selected_.clear(); dirty_ = true;
    }
    ImGui::EndDisabled();
    Card* card = Current();
    if (!card) return;
    if (ImGui::InputText("カード名", &card->name)) dirty_ = true;
    dirty_ |= ImGui::ColorEdit4("カード背景色", card->backgroundColor.data(), ImGuiColorEditFlags_AlphaPreviewHalf);
    auto keyboardButton = [&](const char* label, std::string& target) {
        const auto* selected = card->FindObject(target);
        if (ImGui::BeginCombo(label, selected ? selected->name.c_str() : "指定なし")) {
            if (ImGui::Selectable("指定なし", target.empty())) { target.clear(); dirty_ = true; }
            for (const auto& button : card->objects) if (button.type == ObjectType::Button) {
                ImGui::PushID(button.id.c_str());
                if (ImGui::Selectable(button.name.c_str(), target == button.id)) { target = button.id; dirty_ = true; }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    };
    keyboardButton("Enterボタン", card->enterButtonId);
    keyboardButton("Escapeボタン", card->escapeButtonId);
    ImGui::SeparatorText("部品を追加");
    const ObjectType types[] = {ObjectType::Button, ObjectType::Field, ObjectType::Text, ObjectType::Image,
        ObjectType::Listbox, ObjectType::DropdownList, ObjectType::InputBox, ObjectType::Checkbox, ObjectType::RadioButton};
    for (int i = 0; i < 9; ++i) {
        if (i && i != 4 && i != 7) ImGui::SameLine();
        if (ImGui::Button(ObjectTypeName(types[i]).c_str())) {
            selected_ = stack_.AddObject(card->id, types[i], ObjectTypeName(types[i])).id; dirty_ = true;
        }
    }
    ImGui::SeparatorText("部品一覧");
    for (const auto& o : card->objects) {
        ImGui::PushID(o.id.c_str());
        if (ImGui::Selectable((o.name + "##" + o.id).c_str(), o.id == selected_)) selected_ = o.id;
        ImGui::PopID();
    }
    orderControls("objectOrder", card->objects, selected_);
    ImGui::TextWrapped("部品は一覧の下ほど後に描画されます。");
    if (CardObject* o = card->FindObject(selected_)) {
        ImGui::SeparatorText("プロパティ");
        ImGui::Text("ID: %s (%s)", o->id.c_str(), ObjectTypeName(o->type).c_str());
        dirty_ |= ImGui::InputText("名前", &o->name);
        if (o->type != ObjectType::Listbox && o->type != ObjectType::DropdownList) {
            ImGui::TextUnformatted("文字");
            if (o->type == ObjectType::InputBox) {
                ImGui::SetNextItemWidth(-1);
                dirty_ |= ImGui::InputText("##object-text", &o->text);
            } else dirty_ |= ImGui::InputTextMultiline("##object-text", &o->text, ImVec2(-1, 70));
        }
        if (o->type == ObjectType::Listbox || o->type == ObjectType::DropdownList) {
            std::string lines;
            for (std::size_t i = 0; i < o->items.size(); ++i) { if (i) lines += "\n"; lines += o->items[i]; }
            ImGui::TextUnformatted("項目（1行1項目）");
            if (ImGui::InputTextMultiline("##object-items", &lines, ImVec2(-1,100))) {
                o->items.clear(); std::istringstream input(lines); std::string line;
                while (std::getline(input, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); o->items.push_back(line); }
                if (o->items.empty()) o->selectedIndex = -1;
                else if (o->selectedIndex >= static_cast<int>(o->items.size())) o->selectedIndex = static_cast<int>(o->items.size())-1;
                o->text = o->selectedIndex >= 0 ? o->items[static_cast<std::size_t>(o->selectedIndex)] : ""; dirty_ = true;
            }
            ImGui::TextUnformatted("選択番号（-1=なし）");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputInt("##object-selected-index", &o->selectedIndex)) {
                o->selectedIndex = std::clamp(o->selectedIndex, -1, static_cast<int>(o->items.size())-1);
                o->text = o->selectedIndex >= 0 ? o->items[static_cast<std::size_t>(o->selectedIndex)] : ""; dirty_ = true;
            }
        }
        float pos[2] = {static_cast<float>(o->x), static_cast<float>(o->y)};
        if (ImGui::DragFloat2("位置", pos, 1)) { o->x = pos[0]; o->y = pos[1]; dirty_ = true; }
        float dim[2] = {static_cast<float>(o->width), static_cast<float>(o->height)};
        if (ImGui::DragFloat2("サイズ", dim, 1, 1, 8192)) {
            o->width = std::clamp(dim[0], 1.0f, 8192.0f); o->height = std::clamp(dim[1], 1.0f, 8192.0f); dirty_ = true;
        }
        if(o->type==ObjectType::Checkbox||o->type==ObjectType::RadioButton) {
            bool checked=o->checked;
            if(ImGui::Checkbox("チェック状態", &checked))dirty_ |= card->SetChecked(o->id,checked);
            if(o->type==ObjectType::RadioButton) {
                std::string group=o->group;
                if(ImGui::InputText("グループ",&group))dirty_ |= card->SetRadioGroup(o->id,group);
                ImGui::TextWrapped("同じカードの同じグループは1つだけ選択できます。");
            }
        }
        dirty_ |= ImGui::Checkbox("表示", &o->visible);
        ImGui::SameLine(); dirty_ |= ImGui::Checkbox("有効", &o->enabled);
        if (ImGui::TreeNode("色")) {
            const auto& style = ImGui::GetStyle();
            const ImVec4 background = (o->type == ObjectType::Text || o->type == ObjectType::Image) ? ImVec4(1,1,1,0) :
                style.Colors[o->type == ObjectType::Button ? ImGuiCol_Button : ImGuiCol_FrameBg];
            dirty_ |= EditColor("背景色を指定", o->backgroundColor, background);
            if (o->type != ObjectType::Image)
                dirty_ |= EditColor("文字色を指定", o->textColor, style.Colors[ImGuiCol_Text]);
            else dirty_ |= EditColor("画像の着色を指定", o->tintColor, ImVec4(1,1,1,1));
            dirty_ |= EditColor("枠線色を指定", o->borderColor, style.Colors[ImGuiCol_Border]);
            ImGui::TextWrapped("チェックを外すと標準色に戻ります。Aは不透明度です。");
            ImGui::TreePop();
        }
        if (o->type == ObjectType::Image) {
            int source = o->imageSource == "resource" ? 1 : 0;
            if (ImGui::Combo("画像ソース", &source, "外部ファイル\0内蔵リソース\0")) {
                o->imageSource = source ? "resource" : "file"; dirty_ = true;
            }
            if (source == 0) {
                dirty_ |= ImGui::InputText("画像パス", &o->imagePath);
                if (services_.chooseImage && ImGui::Button("画像を選択")) {
                    const auto imagePath = services_.chooseImage();
                    if (!imagePath.empty()) { o->imagePath = PathToUtf8(imagePath); dirty_ = true; }
                }
                ImGui::TextWrapped("絶対パス、またはデッキファイルからの相対パスを指定します。");
            } else {
                const char* preview = "指定なし";
                for (const auto& resource : services_.imageResources) if (resource.first == o->resourceId) preview = resource.second.c_str();
                if (ImGui::BeginCombo("内蔵画像", preview)) {
                    for (const auto& resource : services_.imageResources)
                        if (ImGui::Selectable(resource.second.c_str(), resource.first == o->resourceId)) {
                            o->resourceId = resource.first; dirty_ = true;
                        }
                    ImGui::EndCombo();
                }
            }
        }
        if(ImGui::Button("部品スクリプトを編集")) {BeginScriptEditor("object",card->id,o->id);return;}
        if (ImGui::Button("部品削除")) {
            const auto id = selected_;
            if (card->enterButtonId == id) card->enterButtonId.clear();
            if (card->escapeButtonId == id) card->escapeButtonId.clear();
            card->objects.erase(std::remove_if(card->objects.begin(), card->objects.end(),
                [&](const CardObject& obj) { return obj.id == id; }), card->objects.end());
            selected_.clear(); dirty_ = true;
        }
    }
    if(ImGui::Button("カードスクリプトを編集")) {BeginScriptEditor("card",card->id);return;}
    if(ImGui::Button("Deckスクリプトを編集")) {BeginScriptEditor("deck");return;}
    ImGui::TextWrapped("Player／実行プレビューでJavaScriptを実行します。app／fs／alert APIが利用できます。");
}
void GuiView::Canvas()
{
    Card* card = Current(); if (!card) return;
    if (magic_) {
        ImGui::Text("%s", card->name.c_str());
        ImGui::BeginChild("card-scroll", ImVec2(0, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar);
    }
    const auto origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(static_cast<float>(stack_.width), static_cast<float>(stack_.height));
    ImGui::Dummy(size);
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size.x, origin.y + size.y);
    draw->AddRectFilled(origin, end, ColorPacked(card->backgroundColor));
    draw->PushClipRect(origin, end, true);
    ImGui::PushClipRect(origin, end, true);
    cardClipMin_=ImGui::GetCurrentWindow()->ClipRect.Min;cardClipMax_=ImGui::GetCurrentWindow()->ClipRect.Max;
    cardOrigin_ = origin; cardExtent_ = size; drawnCardId_ = card->id; drawnGeneration_ = scriptGeneration_;
    const bool editing = magic_ && !test_;
    bool blockKeyboard = scriptConsoleOpen_ || popupWasOpen_ || ImGui::GetIO().WantTextInput ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    bool buttonPushed = false;
    if (!editing && !blockKeyboard &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        // Enterはカード指定ボタンへ配送し、ImGuiの別ボタンへの自動配送を抑止する。
        auto& context = *ImGui::GetCurrentContext();
        context.NavActivateId = context.NavActivateDownId = context.NavActivatePressedId = 0;
    }
    for (auto& o : card->objects) {
        if (!o.visible && !editing) continue;
        ImGui::PushID(o.id.c_str());
        ImVec2 pos(origin.x + static_cast<float>(o.x), origin.y + static_cast<float>(o.y));
        ImVec2 extent(static_cast<float>(o.width), static_cast<float>(o.height));
        ImGui::SetCursorScreenPos(pos);
        int colorCount = 0;
        auto pushColor = [&](ImGuiCol index, const ImVec4& color) { ImGui::PushStyleColor(index, color); ++colorCount; };
        if (o.backgroundColor) {
            const auto base = ColorVector(*o.backgroundColor);
            auto highlight = [&](float amount) {
                return ImVec4(base.x+(1-base.x)*amount, base.y+(1-base.y)*amount, base.z+(1-base.z)*amount, base.w);
            };
            for (auto index : {ImGuiCol_Button, ImGuiCol_FrameBg, ImGuiCol_PopupBg}) pushColor(index, base);
            for (auto index : {ImGuiCol_ButtonHovered, ImGuiCol_FrameBgHovered, ImGuiCol_HeaderHovered}) pushColor(index, highlight(0.15f));
            for (auto index : {ImGuiCol_ButtonActive, ImGuiCol_FrameBgActive, ImGuiCol_Header, ImGuiCol_HeaderActive}) pushColor(index, highlight(0.3f));
        }
        if (o.textColor) pushColor(ImGuiCol_Text, ColorVector(*o.textColor));
        if (o.borderColor) pushColor(ImGuiCol_Border, ColorVector(*o.borderColor));
        if (editing) {
            const auto mouse = ImGui::GetIO().MousePos;
            const bool onHandle = o.id == selected_ && mouse.x >= pos.x+extent.x-10 &&
                mouse.x < pos.x+extent.x && mouse.y >= pos.y+extent.y-10 && mouse.y < pos.y+extent.y;
            if (!onHandle) {
                if (ImGui::InvisibleButton("select", extent)) selected_ = o.id;
            if (ImGui::IsItemActivated()) selected_ = o.id;
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                auto delta = ImGui::GetIO().MouseDelta;
                o.x += delta.x; o.y += delta.y; dirty_ = true;
                pos.x += delta.x; pos.y += delta.y;
            }
            }
            draw->AddRectFilled(pos, ImVec2(pos.x+extent.x,pos.y+extent.y),
                o.backgroundColor ? ColorPacked(*o.backgroundColor) :
                o.type == ObjectType::Button ? IM_COL32(205,220,245,255) : IM_COL32(255,255,255,255));
        }
        if (!editing && o.backgroundColor && (o.type == ObjectType::Text || o.type == ObjectType::Image))
            draw->AddRectFilled(pos, ImVec2(pos.x+extent.x,pos.y+extent.y), ColorPacked(*o.backgroundColor));
        draw->PushClipRect(pos, ImVec2(pos.x+extent.x,pos.y+extent.y), true);
        if (o.type == ObjectType::Image) {
            ImTextureID texture = 0;
            if (o.imageSource == "resource") {
                if (services_.resourceImage && !o.resourceId.empty()) texture = services_.resourceImage(o.resourceId);
            } else {
                auto imagePath = PathFromUtf8(o.imagePath);
                if (imagePath.is_relative()) imagePath = (path_.empty() ? std::filesystem::current_path() : path_.parent_path()) / imagePath;
                if (!o.imagePath.empty()) texture = services_.image(imagePath);
            }
            if (texture) draw->AddImage(texture, pos, ImVec2(pos.x+extent.x,pos.y+extent.y), ImVec2(0,0), ImVec2(1,1),
                o.tintColor ? ColorPacked(*o.tintColor) : IM_COL32_WHITE);
            else draw->AddText(pos, IM_COL32(120,70,70,255), "[Image: 画像を指定]");
        } else if (editing || o.type == ObjectType::Text) {
            draw->AddText(nullptr, 0, ImVec2(pos.x+4,pos.y+4), o.textColor ? ColorPacked(*o.textColor) : IM_COL32(25,30,40,255),
                          o.text.c_str(), nullptr, (std::max)(1.0f,extent.x-8));
        } else {
            ImGui::BeginDisabled(!o.enabled || scriptConsoleOpen_);
            if(o.type==ObjectType::Checkbox||o.type==ObjectType::RadioButton) {
                if(ImGui::InvisibleButton("##toggle",extent,ImGuiButtonFlags_EnableNav) && o.enabled)
                    ChangeChecked(*card,o.id,o.type==ObjectType::Checkbox?!o.checked:true);
                ImGui::RenderNavCursor(ImGui::GetCurrentContext()->LastItemData.Rect,ImGui::GetItemID());
                const auto& style=ImGui::GetStyle();
                const float icon=(std::min)({extent.x,extent.y,ImGui::GetFrameHeight()});
                const ImVec2 iconPos(pos.x,pos.y+(extent.y-icon)/2);
                const bool hovered=ImGui::IsItemHovered(), active=ImGui::IsItemActive();
                const ImU32 background=ImGui::GetColorU32(active?ImGuiCol_FrameBgActive:hovered?ImGuiCol_FrameBgHovered:ImGuiCol_FrameBg);
                const ImU32 checkmark=ImGui::GetColorU32(ImGuiCol_CheckMark);
                if(o.type==ObjectType::Checkbox) {
                    draw->AddRectFilled(iconPos,ImVec2(iconPos.x+icon,iconPos.y+icon),background,style.FrameRounding);
                    if(o.checked)ImGui::RenderCheckMark(draw,ImVec2(iconPos.x+icon*0.2f,iconPos.y+icon*0.2f),checkmark,icon*0.6f);
                } else {
                    const ImVec2 center(iconPos.x+icon/2,iconPos.y+icon/2);
                    draw->AddCircleFilled(center,icon*0.48f,background);
                    if(o.checked)draw->AddCircleFilled(center,icon*0.24f,checkmark);
                }
                draw->AddText(ImVec2(pos.x+icon+style.ItemInnerSpacing.x,pos.y+(extent.y-ImGui::GetFontSize())/2),
                    o.textColor ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImVec4(25.f/255.f,30.f/255.f,40.f/255.f,1.f)),o.text.c_str());
            } else if (o.type == ObjectType::Button) {
                if (ImGui::Button("##button", extent)) { PushButton(*card, o.id); buttonPushed = true; }
                const auto textSize = ImGui::CalcTextSize(o.text.c_str());
                draw->AddText(ImVec2(pos.x+(extent.x-textSize.x)/2, pos.y+(extent.y-textSize.y)/2),
                    o.textColor ? ImGui::GetColorU32(ImGuiCol_Text) :
                    o.enabled ? IM_COL32(255,255,255,255) : IM_COL32(150,150,150,255), o.text.c_str());
            } else if (o.type == ObjectType::Field || o.type == ObjectType::InputBox) {
                const auto id = ImGui::GetID(o.type == ObjectType::Field ? "##field" : "##inputbox");
                blockKeyboard |= ImGui::GetCurrentContext()->ActiveId == id;
                if (o.type == ObjectType::Field) {
                    if (ImGui::InputTextMultiline("##field", &o.text, extent)) dirty_ = true;
                } else {
                    // このInputBoxの処理中だけReturnを無視し、フォーカスと選択を維持する。
                    auto* enter = ImGui::GetKeyData(ImGuiKey_Enter);
                    auto* keypad = ImGui::GetKeyData(ImGuiKey_KeypadEnter);
                    const bool enterDown = enter->Down, keypadDown = keypad->Down;
                    if (ImGui::GetCurrentContext()->ActiveId == id) { enter->Down = false; keypad->Down = false; }
                    const float padding = (std::max)(0.0f, (extent.y-ImGui::GetFontSize())/2);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, padding));
                    ImGui::SetNextItemWidth(extent.x);
                    if (ImGui::InputText("##inputbox", &o.text)) dirty_ = true;
                    ImGui::PopStyleVar(); enter->Down = enterDown; keypad->Down = keypadDown;
                }
                blockKeyboard |= ImGui::IsItemActive() || (ImGui::IsItemFocused() && ImGui::GetCurrentContext()->NavCursorVisible);
            } else if (o.type == ObjectType::Listbox || o.type == ObjectType::DropdownList) {
                auto choose = [&](int index) {
                    if(o.selectedIndex==index)return;
                    ButtonEvent event{card->id,o.id,"change",false,"",scriptGeneration_};
                    event.previousSelectedIndex=o.selectedIndex;
                    event.previousSelectedText=o.selectedIndex>=0?o.items[static_cast<std::size_t>(o.selectedIndex)]:"";
                    o.selectedIndex=index;o.text=o.items[static_cast<std::size_t>(index)];dirty_=true;
                    event.selectedIndex=index;event.selectedText=o.text;
                    buttonEvents_.push_back(std::move(event));
                };
                if (o.type == ObjectType::Listbox) {
                    if (ImGui::BeginListBox("##listbox", extent)) {
                        for (std::size_t i = 0; i < o.items.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            if (ImGui::Selectable(o.items[i].c_str(), o.selectedIndex == static_cast<int>(i))) choose(static_cast<int>(i));
                            ImGui::PopID();
                        }
                        ImGui::EndListBox();
                    }
                } else {
                    const float padding = (std::max)(0.0f, (extent.y-ImGui::GetFontSize())/2);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, padding));
                    ImGui::SetNextItemWidth(extent.x);
                    const char* preview = o.selectedIndex >= 0 ? o.items[static_cast<std::size_t>(o.selectedIndex)].c_str() : "";
                    if (ImGui::BeginCombo("##dropdown", preview)) {
                        for (std::size_t i = 0; i < o.items.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            if (ImGui::Selectable(o.items[i].c_str(), o.selectedIndex == static_cast<int>(i))) choose(static_cast<int>(i));
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopStyleVar();
                }
            }
            ImGui::EndDisabled();
        }
        if (o.borderColor)
            draw->AddRect(pos, ImVec2(pos.x+extent.x,pos.y+extent.y), ColorPacked(*o.borderColor));
        ImGui::PopStyleColor(colorCount);
        draw->PopClipRect();
        if (editing) {
            draw->AddRect(pos, ImVec2(pos.x+extent.x,pos.y+extent.y),
                o.id == selected_ ? IM_COL32(30,120,240,255) : IM_COL32(150,155,165,255), 0, 0, 2);
            if (o.id == selected_) {
                ImVec2 handle(pos.x+extent.x-10,pos.y+extent.y-10);
                ImGui::SetCursorScreenPos(handle);
                ImGui::InvisibleButton("resize", ImVec2(10,10));
                if (ImGui::IsItemActive()) {
                    const auto delta = ImGui::GetIO().MouseDelta;
                    o.width = (std::max)(12.0, o.width + delta.x);
                    o.height = (std::max)(12.0, o.height + delta.y);
                    if (delta.x || delta.y) dirty_ = true;
                }
                draw->AddRectFilled(handle, ImVec2(handle.x+10,handle.y+10), IM_COL32(30,120,240,255));
            }
        }
        ImGui::PopID();
    }
    if (!editing && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() &&
        !ImGui::IsAnyItemHovered() && ImGui::IsMouseHoveringRect(origin, end)) {
        ImGui::ClearActiveID();
        if (ImGui::GetCurrentContext()->NavWindow)
            ImGui::SetNavID(0, ImGuiNavLayer_Main, ImGui::GetCurrentContext()->CurrentFocusScopeId, ImRect());
    }
    if (!editing && !blockKeyboard && !buttonPushed) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
            PushButton(*card, card->enterButtonId);
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) PushButton(*card, card->escapeButtonId);
    }
    popupWasOpen_ = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    ImGui::PopClipRect(); draw->PopClipRect();
    if (magic_) {
        ImGui::SetCursorScreenPos(end); ImGui::Dummy(ImVec2(1,1));
        ImGui::EndChild();
    } else {
        ImGui::SetCursorScreenPos(origin); ImGui::Dummy(size);
    }
}
std::vector<ConsoleCommand> GuiView::TakeConsoleCommands()
{
    std::vector<ConsoleCommand> result;
    const int frame=ImGui::GetCurrentContext()?ImGui::GetFrameCount():0;
    for(auto i=consoleCommands_.begin();i!=consoleCommands_.end();) {
        if(i->readyFrame<=frame){result.push_back(std::move(*i));i=consoleCommands_.erase(i);}else ++i;
    }
    return result;
}
void GuiView::DrawScriptConsole()
{
    const char* title="スクリプト実行";
    if(scriptConsoleOpen_ && !ImGui::IsPopupOpen(title))ImGui::OpenPopup(title);
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(),ImGuiCond_Appearing,ImVec2(0.5f,0.5f));
    const auto& style=ImGui::GetStyle();
    const float height=ImGui::GetFrameHeight()*2+ImGui::GetFrameHeight()+style.ItemSpacing.y+style.WindowPadding.y*2;
    ImGui::SetNextWindowSize(ImVec2((std::min)(520.f,(std::max)(180.f,viewport->WorkSize.x-24.f)),height),ImGuiCond_Always);
    if(ImGui::BeginPopupModal(title,nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings)) {
        if(!scriptConsoleOpen_ || magic_) {ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
        if(scriptConsoleFocus_){ImGui::SetKeyboardFocusHere();scriptConsoleFocus_=false;}
        ImGui::SetNextItemWidth(-1);
        bool run=ImGui::InputTextWithHint("##code","app.setMagic(true);",&scriptConsoleInput_,ImGuiInputTextFlags_EnterReturnsTrue);
        run |= ImGui::Button("Run");
        ImGui::SameLine();
        const bool close=ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape,false);
        if(run) {
            if(!scriptConsoleInput_.empty())consoleCommands_.push_back({scriptConsoleInput_,scriptGeneration_,ImGui::GetFrameCount()+1});
            scriptConsoleOpen_=false;scriptConsoleFocus_=false;
            ImGui::CloseCurrentPopup();ImGui::ClearActiveID();
        }
        if(close){scriptConsoleOpen_=false;ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
}
std::string* GuiView::EditingScript()
{
    if(scriptEditorKind_=="deck")return &stack_.script;
    auto* card=stack_.FindCard(scriptEditorCard_);
    if(!card)return nullptr;
    if(scriptEditorKind_=="card")return &card->script;
    auto* object=card->FindObject(scriptEditorObject_);
    return object?&object->script:nullptr;
}
void GuiView::BeginScriptEditor(const std::string& kind,const std::string& cardId,const std::string& objectId)
{
    if(!magic_ || test_)throw std::runtime_error("Script editing requires Magic edit mode.");
    if(kind!="deck" && kind!="card" && kind!="object")throw std::invalid_argument("Invalid script target.");
    scriptEditorKind_=kind;scriptEditorCard_=cardId;scriptEditorObject_=objectId;
    auto* script=EditingScript();
    if(!script)throw std::invalid_argument("Script target was not found.");
    if(script->find_first_not_of(" \t\r\n")==std::string::npos) {
        if(kind=="card") { *script=DefaultCardScript();dirty_=true; }
        else if(kind=="deck") { *script=DefaultDeckScript();dirty_=true; }
        else { *script=DefaultObjectScript(stack_.FindCard(cardId)->FindObject(objectId)->type);dirty_=true; }
    }
    scriptEditorTitle_=kind=="deck"?"Deck: "+stack_.name:kind=="card"?"カード: "+stack_.FindCard(cardId)->name:"部品: "+stack_.FindCard(cardId)->FindObject(objectId)->name;
    scriptEditing_=true;scriptEditorFocus_=true;
    buttonEvents_.clear();fileDropEvents_.clear();
    if(ImGui::GetCurrentContext())ImGui::ClearActiveID();
}
void GuiView::EndScriptEditor()
{
    scriptEditing_=false;scriptEditorFocus_=false;
    if(ImGui::GetCurrentContext())ImGui::ClearActiveID();
}
void GuiView::DrawScriptEditor()
{
    auto* script=EditingScript();
    if(!script){EndScriptEditor();return;}
    if(ImGui::Button("編集を完了")){EndScriptEditor();return;}
    ImGui::SameLine();
    bool save=ImGui::Button("保存");
    ImGui::SameLine();bool saveAs=ImGui::Button("別名保存");
    ImGui::SameLine();ImGui::TextUnformatted(scriptEditorTitle_.c_str());
    ImGui::Separator();
    if(services_.scriptFont)ImGui::PushFont(services_.scriptFont);
    if(scriptEditorFocus_) {
        ImGui::SetWindowFocus();
        // AllowTabInput rejects tab-navigation activation, including SetKeyboardFocusHere.
        auto& context=*ImGui::GetCurrentContext();
        context.NavActivateId=ImGui::GetID("##script-editor");
        context.NavActivateFlags=ImGuiActivateFlags_PreferInput;
    }
    // Live edits keep close confirmation, Ctrl+S and file saves consistent.
    dirty_ |= ImGui::InputTextMultiline("##script-editor",script,ImGui::GetContentRegionAvail(),ImGuiInputTextFlags_AllowTabInput);
    if(ImGui::IsItemActive())scriptEditorFocus_=false;
    if(services_.scriptFont)ImGui::PopFont();
    if(ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false)) {
        save=true;saveAs=ImGui::GetIO().KeyShift;
    }
    if(save||saveAs)Save(saveAs);
}
void GuiView::Draw()
{
    if(!magic_ && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space,false)) {
        scriptConsoleOpen_=true;scriptConsoleFocus_=true;
        buttonEvents_.clear();fileDropEvents_.clear();
    }
    auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (!magic_) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    }
    ImGui::Begin("ScriptDeck", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse);
    if (!magic_) {
        Canvas();
        ImGui::End();
        ImGui::PopStyleVar(2);
        DrawScriptConsole();
        return;
    }
    if(scriptEditing_) {
        DrawScriptEditor();ImGui::End();return;
    }
    if (ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_O, false)) Open();
        if (magic_ && ImGui::IsKeyPressed(ImGuiKey_S, false)) Save(ImGui::GetIO().KeyShift);
        if (magic_ && ImGui::IsKeyPressed(ImGuiKey_N, false)) New();
    }
    Toolbar(); ImGui::Separator();
    if (magic_ && !test_) {
        ImGui::BeginChild("editor", ImVec2(340,0), ImGuiChildFlags_Borders); Editor(); ImGui::EndChild();
        ImGui::SameLine();
    }
    ImGui::BeginChild("canvas-pane", ImVec2(0,0));
    if(scriptEditing_)DrawScriptEditor();else Canvas();
    ImGui::EndChild();
    ImGui::End();
    DrawScriptConsole();
}

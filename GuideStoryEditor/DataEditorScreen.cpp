#include "DataEditorScreen.h"

namespace gs::app {

namespace {
constexpr platform::Color kBg     {30, 34, 52, 255};
constexpr platform::Color kTitle  {236, 224, 150, 255};
constexpr platform::Color kHint   {150, 160, 180, 255};
constexpr platform::Color kNote   {200, 230, 200, 255};
constexpr platform::Color kBar    {20, 24, 38, 220};

enum EmptyItem   { kNew = 0, kOpen, kBack };
enum ToolbarItem { kTbSave = 0, kTbBack };
} // namespace

DataEditorScreen::DataEditorScreen(std::string title) : m_title(std::move(title)) {
    m_emptyMenu.Add("새로 만들기");
    m_emptyMenu.Add("열기");
    m_emptyMenu.Add("← 메뉴로");
    m_emptyMenu.Layout(kViewW * 0.5f, 320.0f, 320.0f, 60.0f, 18.0f);

    m_toolbar.Add("저장");
    m_toolbar.Add("← 메뉴로");
    m_toolbar.LayoutRow(8.0f, 4.0f, 104.0f, 32.0f, 6.0f);
}

EditorScene DataEditorScreen::Update(const platform::Input& in, float /*dt*/) {
    if (in.WasPressed(platform::Key::Escape)) return EditorScene::Launcher;

    if (!m_active) {
        switch (m_emptyMenu.Update(in)) {
            case kNew:  m_active = true; m_status = "새 문서 — 편집 영역은 추후 구현"; break;
            case kOpen: m_active = true; m_status = "열기 — 데이터 포맷 준비 중(추후 구현)"; break;
            case kBack: return EditorScene::Launcher;
            default:    break;
        }
        return EditorScene::Stay;
    }

    switch (m_toolbar.Update(in)) {
        case kTbSave: m_status = "저장 — 데이터 포맷 준비 중(추후 구현)"; break;
        case kTbBack: return EditorScene::Launcher;
        default:      break;
    }
    return EditorScene::Stay;
}

void DataEditorScreen::Render(platform::IRenderDevice& r) {
    r.Clear(kBg);
    ui::DrawCenteredText(r, m_title, kViewW * 0.5f, 160.0f, 52.0f, kTitle);

    if (!m_active) {
        ui::DrawCenteredText(r, "새로 만들기 또는 열기를 선택하세요",
                             kViewW * 0.5f, 240.0f, 24.0f, kHint);
        m_emptyMenu.Render(r);
        return;
    }

    ui::DrawCenteredText(r, "편집 영역 (추후 구현)", kViewW * 0.5f, kViewH * 0.5f, 30.0f, kHint);
    if (!m_status.empty())
        ui::DrawCenteredText(r, m_status, kViewW * 0.5f, kViewH * 0.5f + 48.0f, 20.0f, kNote);

    r.FillRect({0.0f, 0.0f, kViewW, 40.0f}, kBar);
    m_toolbar.Render(r);
}

} // namespace gs::app

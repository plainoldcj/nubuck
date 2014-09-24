#pragma once

#include <UI\outliner\outliner.h>
#include <world\entity.h>
#include <renderer\text\r_text.h>

namespace W {

// NOTE on terminology: the displayed text is also called 'content'
class ENT_Text : public Entity {
private:
    UI::Outliner::itemHandle_t _outlinerItem;

    R::Text _text;
public:
    ENT_Text();

    UI::OutlinerView* CreateOutlinerView() override;

    const M::Vector2& GetContentSize() const;

    void SetContent(const std::string& content);

    void GetRenderJobs(R::RenderList& renderList);
};

} // namespace W
#pragma once
#include <string>
#include <vector>

#include "htmlq.h"

// A parsed HTML page you can query with CSS selectors (backed by lexbor via htmlq).
//
//   Html page(html);
//   for (Html::Node card : page.all("div.single-rom"))
//       std::string title = Html::text(page.first("h2", card));
//
// Nodes are only valid while the Html object is alive.
class Html {
public:
    using Node = htmlq_node*;

    explicit Html(const std::string& html);
    ~Html();
    Html(const Html&) = delete;
    Html& operator=(const Html&) = delete;

    bool ok() const { return doc_ != nullptr; }

    // Elements matching `css` under `scope` (nullptr = whole page), in page order.
    std::vector<Node> all(const std::string& css, Node scope = nullptr) const;
    // First match, or nullptr.
    Node first(const std::string& css, Node scope = nullptr) const;

    // Text content with whitespace collapsed ("" for nullptr).
    static std::string text(Node node);
    // Attribute value ("" if missing or node is nullptr).
    static std::string attr(Node node, const std::string& name);
    static std::string tag(Node node);
    static Node parent(Node node);

private:
    htmlq_doc* doc_ = nullptr;
};

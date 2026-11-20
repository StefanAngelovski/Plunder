/*
 * htmlq: a tiny C API over lexbor (third_party/lexbor) for querying HTML with
 * CSS selectors. It is the only code that touches lexbor directly; C++ code
 * uses it through the Html class (Html.h).
 *
 * Nodes returned by a document stay valid until htmlq_free() on that document.
 */
#ifndef PLUNDER_HTMLQ_H
#define PLUNDER_HTMLQ_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct htmlq_doc htmlq_doc;
typedef struct htmlq_node htmlq_node;   /* opaque element of a parsed document */

/* Parse an HTML document. Returns NULL on failure. */
htmlq_doc* htmlq_parse(const char* html, size_t len);
void htmlq_free(htmlq_doc* doc);

/* The document itself, usable as a scope for htmlq_select(). */
htmlq_node* htmlq_root(htmlq_doc* doc);

/* Calls cb(node, ctx) for each element under `scope` (NULL = whole document)
 * matching the CSS selector `css`, in document order. cb returns non-zero to
 * stop early. Returns the number of callbacks made, or -1 for an invalid
 * selector. */
int htmlq_select(htmlq_doc* doc, htmlq_node* scope, const char* css,
                 int (*cb)(htmlq_node* node, void* ctx), void* ctx);

/* Text content (entities decoded). malloc'd and NUL-terminated: free() it. */
char* htmlq_text(htmlq_node* node, size_t* len);

/* Attribute value (entities decoded), or NULL if absent. Owned by the document. */
const char* htmlq_attr(htmlq_node* node, const char* name, size_t* len);

/* Lower-case tag name, owned by the document. */
const char* htmlq_tag(htmlq_node* node, size_t* len);

/* Parent element, or NULL. */
htmlq_node* htmlq_parent(htmlq_node* node);

#ifdef __cplusplus
}
#endif

#endif /* PLUNDER_HTMLQ_H */

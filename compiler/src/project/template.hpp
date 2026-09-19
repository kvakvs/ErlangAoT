#pragma once
#include <string>

namespace erlang_aot::project {
// Render the annotated default project with an explicit platform output convention.
std::string starter_template(bool windows);
// Render the default project using the host platform's executable suffix.
std::string starter_template();
} // namespace erlang_aot::project

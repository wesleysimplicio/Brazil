//! LLM Project Mapper — scan a codebase into a structured map so AI agents
//! understand a project before they program it.
//!
//! Zero runtime dependencies; parallel line counting across CPU cores.

pub mod datetime;
pub mod ignore;
pub mod json;
pub mod languages;
pub mod mapper;
pub mod render;
pub mod stacks;
pub mod types;
pub mod walk;

pub use mapper::{build_project_map, Options};
pub use render::render_markdown;
pub use types::{ProjectMap, SCHEMA_VERSION};

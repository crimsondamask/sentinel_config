use egui::{
    Button, Color32, CornerRadius, FontData, FontDefinitions, FontFamily, Frame, Stroke, Vec2,
    Visuals, style::Selection,
};
use egui_async::{Bind, EguiAsyncPlugin};
use egui_extras::{Column, TableBuilder};
use sentinel::Link;

pub const VERSION: &str = env!("CARGO_PKG_VERSION");

enum ConnectionStatus {
    Connected,
    Alarm(String),
}
/// We derive Deserialize/Serialize so we can persist app state on shutdown.
#[derive(serde::Deserialize, serde::Serialize)]
#[serde(default)] // if we add new fields, give them default values when deserializing old state
pub struct App {
    #[serde(skip)] // This how you opt-out of serialization of a field
    selected_row: Option<usize>,
    #[serde(skip)] // This how you opt-out of serialization of a field
    selected_link: Option<usize>,
    #[serde(skip)] // This how you opt-out of serialization of a field
    enable_watch: bool,
    #[serde(skip)] // This how you opt-out of serialization of a field
    links_state: Vec<Link>,
    links_ui_buffer: Vec<Link>,
    #[serde(skip)] // This how you opt-out of serialization of a field
    status: ConnectionStatus,
    #[serde(skip)] // This how you opt-out of serialization of a field
    left_panel_expanded: bool,
    #[serde(skip)] // This how you opt-out of serialization of a field
    links_async_res: Bind<Vec<Link>, String>,
}

impl Default for App {
    fn default() -> Self {
        Self {
            // Example stuff:
            selected_row: None,
            selected_link: None,
            enable_watch: false,
            links_state: Vec::new(),
            links_ui_buffer: Vec::new(),
            left_panel_expanded: true,
            status: ConnectionStatus::Alarm(String::from("Disconnected")),
            links_async_res: Bind::new(false),
        }
    }
}

impl App {
    /// Called once before the first frame.
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        // This is also where you can customize the look and feel of egui using
        // `cc.egui_ctx.set_visuals` and `cc.egui_ctx.set_fonts`.

        let fonts = font_data();
        cc.egui_ctx.set_fonts(fonts);
        egui_extras::install_image_loaders(&cc.egui_ctx);
        let mut visuals = Visuals::light();

        visuals.selection = Selection {
            bg_fill: Color32::from_rgb(81, 129, 154),
            stroke: Stroke::new(1.0, Color32::WHITE),
        };

        visuals.widgets.inactive.weak_bg_fill = Color32::from_rgb(200, 200, 200);
        visuals.widgets.inactive.bg_fill = Color32::from_rgb(220, 220, 220);
        visuals.widgets.inactive.corner_radius = CornerRadius::ZERO;
        visuals.widgets.noninteractive.corner_radius = CornerRadius::ZERO;
        visuals.widgets.active.corner_radius = CornerRadius::ZERO;
        visuals.widgets.hovered.corner_radius = CornerRadius::ZERO;
        visuals.window_corner_radius = CornerRadius::ZERO;
        visuals.window_fill = Color32::from_rgb(197, 197, 197);
        visuals.menu_corner_radius = CornerRadius::ZERO;
        visuals.panel_fill = Color32::from_rgb(220, 220, 220);
        visuals.striped = true;
        visuals.slider_trailing_fill = true;

        cc.egui_ctx.set_visuals(visuals);
        // Load previous app state (if any).
        // Note that you must enable the `persistence` feature for this to work.
        if let Some(storage) = cc.storage {
            eframe::get_value(storage, eframe::APP_KEY).unwrap_or_default()
        } else {
            Default::default()
        }
    }
}

impl eframe::App for App {
    /// Called by the framework to save state before shutdown.
    fn save(&mut self, storage: &mut dyn eframe::Storage) {
        eframe::set_value(storage, eframe::APP_KEY, self);
    }

    /// Called each time the UI needs repainting, which may be many times per second.
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Put your widgets into a `SidePanel`, `TopBottomPanel`, `CentralPanel`, `Window` or `Area`.
        // For inspiration and more examples, go to https://emilk.github.io/egui

        ctx.plugin_or_default::<EguiAsyncPlugin>();

        ctx.request_repaint();
        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            // The top panel is often a good place for a menu bar:

            egui::MenuBar::new().ui(ui, |ui| {
                // NOTE: no File->Quit on web pages!
                let is_web = cfg!(target_arch = "wasm32");
                if !is_web {
                    //ui.image(egui::include_image!("../assets/sentinel.svg"));
                    let logo_img = egui::Image::new(egui::include_image!("../assets/sentinel.svg"))
                        .fit_to_exact_size(Vec2::new(80.0, 30.0));
                    ui.menu_image_button(logo_img, |ui| {
                        if ui.button("Start").clicked() {}
                        if ui.button("Stop").clicked() {}
                        ui.separator();
                        if ui.button("Quit").clicked() {
                            ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                        }
                    });

                    /*
                    *
                    ui.add(
                        egui::Image::new(egui::include_image!("../assets/sentinel.svg"))
                            .fit_to_exact_size(Vec2::new(80.0, 30.0)),
                    );
                    */
                    ui.menu_button("Project", |ui| {});

                    ui.add_space(16.0);
                }

                ui.with_layout(egui::Layout::right_to_left(egui::Align::RIGHT), |ui| {
                    ui.horizontal(|ui| {
                        ui.spacing_mut().item_spacing.x = 0.0;
                        ui.label(format!("Sentinel Configurator V{}", VERSION));
                    });
                });
                //egui::widgets::global_theme_preference_buttons(ui);
            });
            egui::MenuBar::new().ui(ui, |ui| {
                if ui
                    .button(format!(
                        "{}",
                        egui_phosphor::regular::ARROWS_IN_LINE_HORIZONTAL
                    ))
                    .clicked()
                {
                    self.left_panel_expanded = !self.left_panel_expanded;
                }
                ui.toggle_value(
                    &mut self.enable_watch,
                    format!("{}", egui_phosphor::regular::EYEGLASSES),
                );

                ui.with_layout(egui::Layout::right_to_left(egui::Align::RIGHT), |ui| {
                    ui.horizontal(|ui| {
                        ui.spacing_mut().item_spacing.x = 0.0;
                        /*
                        *
                        ui.add(
                            egui::Image::new(egui::include_image!("../assets/sentinel.svg"))
                                .fit_to_exact_size(Vec2::new(80.0, 30.0)),
                        );
                        */
                    });
                });
            });
        });
        let status_frame = match self.status {
            ConnectionStatus::Alarm(_) => Frame::new().fill(Color32::ORANGE),
            ConnectionStatus::Connected => Frame::new().fill(Color32::DARK_GREEN),
        };
        if self.enable_watch {
            egui::TopBottomPanel::top("status_panel")
                .frame(status_frame)
                .show(ctx, |ui| {
                    ui.vertical_centered(|ui| match &self.status {
                        ConnectionStatus::Alarm(e) => {
                            ui.strong(format!("{} {e}", egui_phosphor::regular::WARNING));
                        }
                        ConnectionStatus::Connected => {
                            ui.strong(format!(
                                "{} Connected",
                                egui_phosphor::regular::CHECK_CIRCLE
                            ));
                        }
                    });
                });
        }

        egui::SidePanel::left("Left panel").show_animated(ctx, self.left_panel_expanded, |ui| {
            ui.label(format!("{} Links", egui_phosphor::regular::PLUGS));
            ui.separator();
            ui.vertical_centered_justified(|ui| {
                if let Some(selected_link) = self.selected_link {
                    for (i, link) in self.links_state.iter().enumerate() {
                        let link_name = match link {
                            Link::Device(device) => device.name.clone(),
                            _ => String::from("No Name"),
                        };
                        if selected_link == i {
                            ui.add(Button::new(format!("{}", link_name)).selected(true));
                        } else {
                            if ui.button(format!("{}", link_name)).clicked() {
                                self.selected_link = Some(i);
                            }
                        }
                    }
                } else {
                    for (i, link) in self.links_state.iter().enumerate() {
                        let link_name = match link {
                            Link::Device(device) => device.name.clone(),
                            _ => String::from("No Name"),
                        };
                        if ui.button(format!("{}", link_name)).clicked() {
                            self.selected_link = Some(i);
                        }
                    }
                }
            });
        });
        egui::CentralPanel::default().show(ctx, |ui| {
            // The central panel the region left after adding TopPanel's and SidePanel's
            ui.heading("Sentinel Configurator");
            ui.separator();

            if self.enable_watch {
                let refresh_rate = 0.5;
                let time_until_refresh = self
                    .links_async_res
                    .request_every_sec(fetch_links, refresh_rate);
                if let Some(links) = self.links_async_res.read() {
                    match links {
                        Ok(links) => {
                            self.links_state = links.clone();
                            self.status = ConnectionStatus::Connected;
                        }
                        Err(e) => {
                            self.status = ConnectionStatus::Alarm(e.to_string());
                        }
                    }
                }
            }
            let available_height = ui.available_height();
            let mut table = TableBuilder::new(ui)
                .striped(true)
                .resizable(true)
                .cell_layout(egui::Layout::right_to_left(egui::Align::Center))
                .column(Column::auto())
                .column(Column::auto())
                .column(Column::auto())
                .column(Column::auto())
                .column(Column::auto())
                .column(Column::auto())
                .scroll_bar_visibility(egui::scroll_area::ScrollBarVisibility::AlwaysVisible)
                .vscroll(true)
                .min_scrolled_height(0.0)
                .max_scroll_height(available_height);

            table = table.sense(egui::Sense::click());
            table
                .header(40.0, |mut header| {
                    header.col(|ui| {
                        ui.label("ID");
                    });
                    header.col(|ui| {
                        ui.label("TK");
                    });
                    header.col(|ui| {
                        ui.label("NAME");
                    });
                    header.col(|ui| {
                        ui.label("TYPE");
                    });
                    header.col(|ui| {
                        ui.label("VALUE");
                    });
                    header.col(|ui| {
                        ui.label("STATUS");
                    });
                })
                .body(|body| {
                    let row_height = 20.0;
                    // TODO
                    // Better error handling
                    if let Some(selected_link) = self.selected_link {
                        let link = self.links_state.iter().nth(selected_link);
                        match link {
                            Some(link) => match link {
                                Link::Device(link) => {
                                    let num_rows = link.tags.len();

                                    body.rows(row_height, num_rows, |mut row| {
                                        let index = row.index();
                                        let is_row_selected = Some(index) == self.selected_row;
                                        if is_row_selected {
                                            row.set_selected(true);
                                        } else {
                                            row.set_selected(false);
                                        }

                                        row.col(|ui| {
                                            let tag_id = link.tags[index].id;
                                            ui.label(format!("{}", tag_id));
                                        });
                                        row.col(|ui| {
                                            let tag_tk = link.tags[index].tk.clone();
                                            ui.label(format!("{}", tag_tk));
                                        });
                                        row.col(|ui| {
                                            let tag_name = link.tags[index].name.clone();
                                            ui.label(format!("{}", tag_name));
                                        });
                                        row.col(|ui| {
                                            let tag_value_type = link.tags[index].value.clone();
                                            match tag_value_type {
                                                sentinel::TagValue::Int(_) => {
                                                    ui.label("INT");
                                                }
                                                sentinel::TagValue::Real(_) => {
                                                    ui.label("REAL");
                                                }
                                                sentinel::TagValue::Dint(_) => {
                                                    ui.label("DINT");
                                                }
                                                sentinel::TagValue::Bit(_) => {
                                                    ui.label("BIT");
                                                }
                                            }
                                        });
                                        row.col(|ui| {
                                            let tag_value_type = link.tags[index].value.clone();
                                            match tag_value_type {
                                                sentinel::TagValue::Int(v) => {
                                                    ui.label(format!("{v}"));
                                                }
                                                sentinel::TagValue::Real(v) => {
                                                    ui.label(format!("{:.3}", v));
                                                }
                                                sentinel::TagValue::Dint(v) => {
                                                    ui.label(format!("{v}"));
                                                }
                                                sentinel::TagValue::Bit(v) => {
                                                    ui.label(format!("{v}"));
                                                }
                                            }
                                        });
                                        row.col(|ui| {
                                            let tag_status = link.tags[index].status.clone();
                                            match tag_status {
                                                sentinel::TagStatus::Normal => {
                                                    ui.label(format!("OK"));
                                                }
                                                sentinel::TagStatus::Alarm => {
                                                    ui.label(format!("ALARM"));
                                                }
                                                sentinel::TagStatus::Warn => {
                                                    ui.label(format!("WARNING"));
                                                }
                                                sentinel::TagStatus::Error(e) => {
                                                    ui.label(format!("ERROR: {e}"));
                                                }
                                            }
                                        });
                                        if row.response().clicked() {
                                            self.selected_row = Some(index);
                                            println!("{:?}", self.selected_row);
                                        }
                                    });
                                }
                                _ => {}
                            },
                            None => {}
                        }
                    }
                });
        });

        egui::TopBottomPanel::bottom("bottom_panel").show(ctx, |ui| {
            ui.with_layout(egui::Layout::right_to_left(egui::Align::RIGHT), |ui| {
                powered_by_egui_and_eframe(ui);
            });
        });
    }
}

fn powered_by_egui_and_eframe(ui: &mut egui::Ui) {
    ui.horizontal(|ui| {
        ui.spacing_mut().item_spacing.x = 0.0;
        ui.label("Powered by Sentinel V1.0");
    });
}

fn font_data() -> FontDefinitions {
    let mut fonts = FontDefinitions::default();
    egui_phosphor::add_to_fonts(&mut fonts, egui_phosphor::Variant::Regular);

    fonts.font_data.insert(
        "plex".to_owned(),
        std::sync::Arc::new(FontData::from_static(include_bytes!("../assets/plex.ttf"))),
    );

    fonts
        .families
        .get_mut(&FontFamily::Proportional)
        .unwrap()
        .insert(0, "plex".to_owned());

    fonts
        .families
        .get_mut(&FontFamily::Monospace)
        .unwrap()
        .push("plex".to_owned());
    fonts
}
async fn fetch_links() -> Result<Vec<Link>, String> {
    let url = "http://127.0.0.1:3000/api/get_links_config";
    let links = reqwest::get(url)
        .await
        .map_err(|e| e.to_string())?
        .json()
        .await
        .map_err(|e| e.to_string())?;

    Ok(links)
}
async fn fetch_device_link_config() -> Result<Link, String> {
    let url = "http://127.0.0.1:3000/api/get_device_link_config";
    let client = reqwest::Client::new();
    let post_data = sentinel::LinkIdQuery { link_id: 0 };
    let res = client
        .post(url)
        .json(&post_data)
        .send()
        .await
        .map_err(|e| e.to_string())?;

    let link = res.json::<Link>().await.map_err(|e| e.to_string())?;

    Ok(link)
}

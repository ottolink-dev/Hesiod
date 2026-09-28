/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include <QColor>
#include <QDialog>
#include <QPoint>

class QButtonGroup;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace hesiod
{

struct AppSettings;

// Application settings: a frameless dialog in the application's own chrome,
// with a searchable section list on the left and one scrolling page per
// section.
//
// Nothing is applied while the user explores: every control edits a draft,
// modified rows are marked, and only Apply writes the draft into
// HSD_CTX.app_settings (and runs the side effects: live rescale, theme,
// autosave...). Discard, or closing and choosing to discard, leaves the
// running application untouched. Rows that only take effect on the next launch
// carry a "Restart" tag, and applying one of them raises a banner saying so.
class AppSettingsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit AppSettingsWindow(QWidget *parent = nullptr);
  ~AppSettingsWindow() override;

  // side effects a committed setting needs, run once per Apply
  enum Effect : unsigned
  {
    NoEffect = 0,
    Autosave = 1u << 0,
    Animations = 1u << 1,
    Theme = 1u << 2,
    Palette = 1u << 3,
    Scale = 1u << 4,
    ViewportToolbar = 1u << 5
  };

  struct Binding;

  // a control plus the draft it edits
  struct Control
  {
    QWidget                 *widget = nullptr;
    QWidget                 *focus = nullptr; // what Enter-from-search focuses
    std::shared_ptr<Binding> binding;
  };

protected:
  void done(int result) override;
  void keyPressEvent(QKeyEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  struct Row;
  struct Card;

  // --- page / card / row construction
  QVBoxLayout *add_page(const QString &title, const QString &subtitle);
  QVBoxLayout *add_card(QVBoxLayout *page, const QString &heading = QString());
  void         add_row(QVBoxLayout   *card,
                       const QString &label,
                       const QString &description,
                       const Control &control,
                       bool           needs_restart = false,
                       const QString &keywords = QString());

  // --- controls editing a draft of one setting
  Control make_toggle(std::function<bool &(AppSettings &)> access,
                      unsigned                             effects = NoEffect);
  Control make_int(std::function<int &(AppSettings &)> access,
                   int                                 min,
                   int                                 max,
                   const QString                      &suffix = QString(),
                   unsigned                            effects = NoEffect);
  Control make_choice(std::function<int &(AppSettings &)>         access,
                      const std::vector<std::pair<int, QString>> &items,
                      unsigned                                    effects = NoEffect);
  Control make_color(std::function<QColor &(AppSettings &)> access,
                     unsigned                               effects = Theme);
  Control make_scale();

  void setup_pages();
  void apply_stylesheet();
  void fit_to_screen();

  // --- draft handling
  int  dirty_count() const;
  void apply_changes();
  void discard_changes();
  void load_defaults();
  void reset_everything(); // the whole settings file, after a confirmation
  // live side effects of committed settings; a note when one could not apply
  QString apply_effects(unsigned effects);
  void    update_state();
  bool    confirm_close();

  // --- search
  void apply_filter(const QString &query);
  void jump_to_best_match();

  QStackedWidget *pages = nullptr;
  QButtonGroup   *nav_group = nullptr;
  QVBoxLayout    *nav_layout = nullptr;
  QLineEdit      *search = nullptr;
  QLabel         *search_status = nullptr;
  QLabel         *footer_status = nullptr;
  QPushButton    *apply_button = nullptr;
  QPushButton    *discard_button = nullptr;
  QFrame         *restart_banner = nullptr;
  QLabel         *restart_label = nullptr;
  QLabel         *scale_note = nullptr;

  std::vector<std::unique_ptr<Row>>  rows;
  std::vector<std::unique_ptr<Card>> cards;
  std::vector<QString>               page_titles;
  Row                               *best_match = nullptr;

  QPoint drag_offset;
  bool   dragging = false;
};

} // namespace hesiod

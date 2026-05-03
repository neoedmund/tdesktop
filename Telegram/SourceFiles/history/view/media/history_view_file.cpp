/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_file.h"

#include "lang/lang_keys.h"
#include "ui/text/format_values.h"
#include "history/history_item.h"
#include "history/history.h"
#include "history/view/history_view_element.h"
#include "data/data_document.h"
#include "data/data_file_click_handler.h"
#include "data/data_session.h"
#include "styles/style_chat.h"
#include <QDateTime>

#include "core/application.h"
#include "data/data_peer.h"
#include "data/data_channel.h"
#include "main/main_session.h"
#include "data/data_session.h"

void LogMediaClick(const FullMsgId& id, const QString& type) {
    const QString logPath = QDir::homePath() + "/tg_media_clicks.log";

    QFile file(logPath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QString msgText;
    if (const auto session = Core::App().maybePrimarySession()) {
        if (const auto item = session->data().message(id)) {
            msgText = item->originalText().text;
        }
    }

    // === Prefer username link[](https://t.me/engChatId/2794) when possible ===
    QString link;
   if (const auto session = Core::App().maybePrimarySession()) {
        if (const auto peer = session->data().peer(id.peer)) {
            if (const auto channel = peer->asChannel()) {
                if (const auto &username = channel->username(); !username.isEmpty()) {
                    link = QString("https://t.me/%1/%2")
                        .arg(username)
                        .arg(id.msg.bare);
                }
            }
        }
    }

    // Fallback to numeric link only if no username
    if (link.isEmpty()) {
        link = QString("https://t.me/c/%1/%2")
           .arg(id.peer.value)
            .arg(id.msg.bare);
    }
    
		QTextStream out(&file);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
        << " | " << type
        << " | " << link
        << " | Text: " << msgText.replace('\n', ' ').replace('\r', ' ').trimmed()
        << "\n";
}

namespace HistoryView {

bool File::toggleSelectionByHandlerClick(const ClickHandlerPtr &p) const {
	return p == _openl || p == _savel || p == _cancell;
}

bool File::dragItemByHandler(const ClickHandlerPtr &p) const {
	return p == _openl || p == _savel || p == _cancell;
}

void File::clickHandlerActiveChanged(const ClickHandlerPtr &p, bool active) {
	if (p == _savel || p == _cancell) {
		if (active && !dataLoaded()) {
			ensureAnimation();
			_animation->a_thumbOver.start([=] { repaint(); }, 0., 1., st::msgFileOverDuration);
		} else if (!active && _animation && !dataLoaded()) {
			_animation->a_thumbOver.start([=] { repaint(); }, 1., 0., st::msgFileOverDuration);
		}
	}
}

void File::clickHandlerPressedChanged(
		const ClickHandlerPtr &handler,
		bool pressed) {
	repaint();
}

void File::setLinks(
		FileClickHandlerPtr &&openl,
		FileClickHandlerPtr &&savel,
		FileClickHandlerPtr &&cancell) {
	_openl = std::move(openl);
	_savel = std::move(savel);
	_cancell = std::move(cancell);
}

void File::refreshParentId(not_null<HistoryItem*> realParent) {
	const auto contextId = realParent->fullId();
	if (_openl) {
		_openl->setMessageId(contextId);
	}
	if (_savel) {
		_savel->setMessageId(contextId);
	}
	if (_cancell) {
		_cancell->setMessageId(contextId);
	}
}

void File::setStatusSize(
		int64 newSize,
		int64 fullSize,
		TimeId duration,
		TimeId realDuration) const {
	_statusSize = newSize;
	if (_statusSize == Ui::FileStatusSizeReady) {
		_statusText = (duration >= 0) ? Ui::FormatDurationAndSizeText(duration, fullSize) : (duration < -1 ? Ui::FormatGifAndSizeText(fullSize) : Ui::FormatSizeText(fullSize));
	} else if (_statusSize == Ui::FileStatusSizeLoaded) {
		_statusText = (duration >= 0) ? Ui::FormatDurationText(duration) : (duration < -1 ? u"GIF"_q : Ui::FormatSizeText(fullSize));
	} else if (_statusSize == Ui::FileStatusSizeFailed) {
		_statusText = tr::lng_attach_failed(tr::now);
	} else if (_statusSize >= 0) {
		_statusText = Ui::FormatDownloadText(_statusSize, fullSize);
	} else {
		_statusText = Ui::FormatPlayedText(-_statusSize - 1, realDuration);
	}
}

void File::radialAnimationCallback(crl::time now) const {
	const auto updated = [&] {
		return _animation->radial.update(
			dataProgress(),
			dataFinished(),
			now);
	}();
	if (!anim::Disabled() || updated) {
		repaint();
	}
	if (!_animation->radial.animating()) {
		checkAnimationFinished();
	}
}

void File::ensureAnimation() const {
	if (!_animation) {
		_animation = std::make_unique<AnimationData>([=](crl::time now) {
			radialAnimationCallback(now);
		});
	}
}

void File::checkAnimationFinished() const {
	if (_animation && !_animation->a_thumbOver.animating() && !_animation->radial.animating()) {
		if (dataLoaded()) {
			_animation.reset();
		}
	}
}
void File::setDocumentLinks(
		not_null<DocumentData*> document,
		not_null<HistoryItem*> realParent,
		Fn<bool()> openHook) {
	const auto context = realParent->fullId();
	setLinks(
		std::make_shared<DocumentOpenClickHandler>(
			document,
			crl::guard(this, [=](FullMsgId id) {

			// === YOUR LOG FOR VIDEO CLICK GOES HERE ===
			if (document->isVideoFile() || document->isVideoMessage()) {
				LogMediaClick(id, "video");
			}

				if (!openHook || !openHook()) {
					_parent->delegate()->elementOpenDocument(document, id);
				}
			}),
			context),
		std::make_shared<DocumentSaveClickHandler>(document, context),
		std::make_shared<DocumentCancelClickHandler>(
			document,
			crl::guard(this, [=](FullMsgId id) {
				_parent->delegate()->elementCancelUpload(id);
			}),
			context));
}

File::~File() = default;

} // namespace HistoryView

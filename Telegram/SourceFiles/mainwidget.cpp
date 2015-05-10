/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014 John Preston, https://desktop.telegram.org
*/
#include "stdafx.h"
#include "style.h"
#include "lang.h"

#include "boxes/addcontactbox.h"
#include "application.h"
#include "window.h"
#include "settingswidget.h"
#include "mainwidget.h"
#include "boxes/confirmbox.h"

#include "localstorage.h"

#include "audio.h"

TopBarWidget::TopBarWidget(MainWidget *w) : TWidget(w),
	a_over(0), _drawShadow(true), _selCount(0), _selStrLeft(-st::topBarButton.width / 2), _selStrWidth(0), _animating(false),
	_clearSelection(this, lang(lng_selected_clear), st::topBarButton),
	_forward(this, lang(lng_selected_forward), st::topBarActionButton),
	_delete(this, lang(lng_selected_delete), st::topBarActionButton),
	_selectionButtonsWidth(_clearSelection.width() + _forward.width() + _delete.width()), _forwardDeleteWidth(qMax(_forward.textWidth(), _delete.textWidth())),
	_info(this, lang(lng_topbar_info), st::topBarButton),
	_edit(this, lang(lng_profile_edit_contact), st::topBarButton),
	_leaveGroup(this, lang(lng_profile_delete_and_exit), st::topBarButton),
	_addContact(this, lang(lng_profile_add_contact), st::topBarButton),
	_deleteContact(this, lang(lng_profile_delete_contact), st::topBarButton),
	_mediaType(this, lang(lng_media_type), st::topBarButton) {

	connect(&_forward, SIGNAL(clicked()), this, SLOT(onForwardSelection()));
	connect(&_delete, SIGNAL(clicked()), this, SLOT(onDeleteSelection()));
	connect(&_clearSelection, SIGNAL(clicked()), this, SLOT(onClearSelection()));
	connect(&_info, SIGNAL(clicked()), this, SLOT(onInfoClicked()));
	connect(&_addContact, SIGNAL(clicked()), this, SLOT(onAddContact()));
	connect(&_deleteContact, SIGNAL(clicked()), this, SLOT(onDeleteContact()));
	connect(&_edit, SIGNAL(clicked()), this, SLOT(onEdit()));
	connect(&_leaveGroup, SIGNAL(clicked()), this, SLOT(onDeleteAndExit()));

	setCursor(style::cur_pointer);
	showAll();
}

void TopBarWidget::onForwardSelection() {
	if (App::main()) App::main()->forwardSelectedItems();
}

void TopBarWidget::onDeleteSelection() {
	if (App::main()) App::main()->deleteSelectedItems();
}

void TopBarWidget::onClearSelection() {
	if (App::main()) App::main()->clearSelectedItems();
}

void TopBarWidget::onInfoClicked() {
	PeerData *p = App::main() ? App::main()->historyPeer() : 0;
	if (p) App::main()->showPeerProfile(p);
}

void TopBarWidget::onAddContact() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	UserData *u = (p && !p->chat) ? p->asUser() : 0;
	if (u) App::wnd()->showLayer(new AddContactBox(u->firstName, u->lastName, u->phone));
}

void TopBarWidget::onEdit() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	if (p) App::wnd()->showLayer(new AddContactBox(p));
}

void TopBarWidget::onDeleteContact() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	UserData *u = (p && !p->chat) ? p->asUser() : 0;
	if (u) {
		ConfirmBox *box = new ConfirmBox(lng_sure_delete_contact(lt_contact, p->name));
		connect(box, SIGNAL(confirmed()), this, SLOT(onDeleteContactSure()));
		App::wnd()->showLayer(box);
	}
}

void TopBarWidget::onDeleteContactSure() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	UserData *u = (p && !p->chat) ? p->asUser() : 0;
	if (u) {
		App::main()->showPeer(0, 0, true);
		App::wnd()->hideLayer();
		MTP::send(MTPcontacts_DeleteContact(u->inputUser), App::main()->rpcDone(&MainWidget::deletedContact, u));
	}
}

void TopBarWidget::onDeleteAndExit() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	ChatData *c = (p && p->chat) ? p->asChat() : 0;
	if (c) {
		ConfirmBox *box = new ConfirmBox(lng_sure_delete_and_exit(lt_group, p->name));
		connect(box, SIGNAL(confirmed()), this, SLOT(onDeleteAndExitSure()));
		App::wnd()->showLayer(box);
	}
}

void TopBarWidget::onDeleteAndExitSure() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	ChatData *c = (p && p->chat) ? p->asChat() : 0;
	if (c) {
		App::main()->showPeer(0, 0, true);
		App::wnd()->hideLayer();
		MTP::send(MTPmessages_DeleteChatUser(MTP_int(p->id & 0xFFFFFFFF), App::self()->inputUser), App::main()->rpcDone(&MainWidget::deleteHistory, p), App::main()->rpcFail(&MainWidget::leaveChatFailed, p));
	}
}

void TopBarWidget::enterEvent(QEvent *e) {
	a_over.start(1);
	anim::start(this);
}

void TopBarWidget::enterFromChildEvent(QEvent *e) {
	a_over.start(1);
	anim::start(this);
}

void TopBarWidget::leaveEvent(QEvent *e) {
	a_over.start(0);
	anim::start(this);
}

void TopBarWidget::leaveToChildEvent(QEvent *e) {
	a_over.start(0);
	anim::start(this);
}

bool TopBarWidget::animStep(float64 ms) {
	float64 dt = ms / st::topBarDuration;
	bool res = true;
	if (dt >= 1) {
		a_over.finish();
		res = false;
	} else {
		a_over.update(dt, anim::linear);
	}
	update();
	return res;
}

void TopBarWidget::enableShadow(bool enable) {
	_drawShadow = enable;
}

void TopBarWidget::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	if (e->rect().top() < st::topBarHeight) { // optimize shadow-only drawing
		p.fillRect(QRect(0, 0, width(), st::topBarHeight), st::topBarBG->b);
		if (_clearSelection.isHidden()) {
			p.save();
			main()->paintTopBar(p, a_over.current(), _info.isHidden() ? 0 : _info.width());
			p.restore();
		} else {
			p.setFont(st::linkFont->f);
			p.setPen(st::btnDefLink.color->p);
			p.drawText(_selStrLeft, st::topBarButton.textTop + st::linkFont->ascent, _selStr);
		}
	}
	if (_drawShadow) {
		int32 shadowCoord = 0;
		float64 shadowOpacity = 1.;
		main()->topBarShadowParams(shadowCoord, shadowOpacity);

		p.setOpacity(shadowOpacity);
		if (cWideMode()) {
			p.fillRect(shadowCoord + st::titleShadow, st::topBarHeight, width() - st::titleShadow, st::titleShadow, st::titleShadowColor->b);
		} else {
			p.fillRect(shadowCoord, st::topBarHeight, width(), st::titleShadow, st::titleShadowColor->b);
		}
	}
}

void TopBarWidget::mousePressEvent(QMouseEvent *e) {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	if (e->button() == Qt::LeftButton && e->pos().y() < st::topBarHeight && (p || !_selCount)) {
		emit clicked();
	}
}

void TopBarWidget::resizeEvent(QResizeEvent *e) {
	int32 r = width();
	if (!_forward.isHidden()) {
		int32 fullW = r - (_selectionButtonsWidth + (_selStrWidth - st::topBarButton.width) + st::topBarActionSkip);
		int32 selectedClearWidth = st::topBarButton.width, forwardDeleteWidth = st::topBarActionButton.width - _forwardDeleteWidth, skip = st::topBarActionSkip;
		while (fullW < 0) {
			int fit = 0;
			if (selectedClearWidth < -2 * (st::topBarMinPadding + 1)) {
				fullW += 4;
				selectedClearWidth += 2;
			} else if (selectedClearWidth < -2 * st::topBarMinPadding) {
				fullW += (-2 * st::topBarMinPadding - selectedClearWidth) * 2;
				selectedClearWidth = -2 * st::topBarMinPadding;
			} else {
				++fit;
			}
			if (fullW >= 0) break;

			if (forwardDeleteWidth > 2 * (st::topBarMinPadding + 1)) {
				fullW += 4;
				forwardDeleteWidth -= 2;
			} else if (forwardDeleteWidth > 2 * st::topBarMinPadding) {
				fullW += (forwardDeleteWidth - 2 * st::topBarMinPadding) * 2;
				forwardDeleteWidth = 2 * st::topBarMinPadding;
			} else {
				++fit;
			}
			if (fullW >= 0) break;

			if (skip > st::topBarMinPadding) {
				--skip;
				++fullW;
			} else {
				++fit;
			}
			if (fullW >= 0 || fit >= 3) break;
		}
		_clearSelection.setWidth(selectedClearWidth);
		_forward.setWidth(_forwardDeleteWidth + forwardDeleteWidth);
		_delete.setWidth(_forwardDeleteWidth + forwardDeleteWidth);
		_selStrLeft = -selectedClearWidth / 2;

		int32 availX = _selStrLeft + _selStrWidth, availW = r - (_clearSelection.width() + selectedClearWidth / 2) - availX;
		_forward.move(availX + (availW - _forward.width() - _delete.width() - skip) / 2, (st::topBarHeight - _forward.height()) / 2);
		_delete.move(availX + (availW + _forward.width() - _delete.width() + skip) / 2, (st::topBarHeight - _forward.height()) / 2);
		_clearSelection.move(r -= _clearSelection.width(), 0);
	}
	if (!_info.isHidden()) _info.move(r -= _info.width(), 0);
	if (!_deleteContact.isHidden()) _deleteContact.move(r -= _deleteContact.width(), 0);
	if (!_leaveGroup.isHidden()) _leaveGroup.move(r -= _leaveGroup.width(), 0);
	if (!_edit.isHidden()) _edit.move(r -= _edit.width(), 0);
	if (!_addContact.isHidden()) _addContact.move(r -= _addContact.width(), 0);
	if (!_mediaType.isHidden()) _mediaType.move(r -= _mediaType.width(), 0);
}

void TopBarWidget::startAnim() {
	_info.hide();
	_edit.hide();
	_leaveGroup.hide();
	_addContact.hide();
	_deleteContact.hide();
    _clearSelection.hide();
    _delete.hide();
    _forward.hide();
	_mediaType.hide();
    _animating = true;
}

void TopBarWidget::stopAnim() {
    _animating = false;
    showAll();
}

void TopBarWidget::showAll() {
    if (_animating) {
        resizeEvent(0);
        return;
    }
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	if (p && (p->chat || p->asUser()->contact >= 0)) {
		if (p->chat) {
			if (p->asChat()->forbidden) {
				_edit.hide();
			} else {
				_edit.show();
			}
			_leaveGroup.show();
			_addContact.hide();
			_deleteContact.hide();
		} else if (p->asUser()->contact > 0) {
			_edit.show();
			_leaveGroup.hide();
			_addContact.hide();
			_deleteContact.show();
		} else {
			_edit.hide();
			_leaveGroup.hide();
			_addContact.show();
			_deleteContact.hide();
		}
		_clearSelection.hide();
		_info.hide();
		_delete.hide();
		_forward.hide();
		_mediaType.hide();
	} else {
		_edit.hide();
		_leaveGroup.hide();
		_addContact.hide();
		_deleteContact.hide();
		if (!p && _selCount) {
			_clearSelection.show();
			_delete.show();
			_forward.show();
			_mediaType.hide();
		} else {
			_clearSelection.hide();
			_delete.hide();
			_forward.hide();
			if (App::main() && App::main()->mediaTypeSwitch()) {
				_mediaType.show();
			} else {
				_mediaType.hide();
			}
		}
        if (App::main() && App::main()->historyPeer() && !p && _clearSelection.isHidden() && !cWideMode()) {
			_info.show();
		} else {
			_info.hide();
		}
	}
	resizeEvent(0);
}

void TopBarWidget::showSelected(uint32 selCount) {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	_selCount = selCount;
	_selStr = (_selCount > 0) ? lng_selected_count(lt_count, _selCount) : QString();
	_selStrWidth = st::btnDefLink.font->m.width(_selStr);
	setCursor((!p && _selCount) ? style::cur_default : style::cur_pointer);
	showAll();
}

FlatButton *TopBarWidget::mediaTypeButton() {
	return &_mediaType;
}

MainWidget *TopBarWidget::main() {
	return static_cast<MainWidget*>(parentWidget());
}

MainWidget::MainWidget(Window *window) : QWidget(window), _started(0), failedObjId(0), _toForwardNameVersion(0), _dialogsWidth(st::dlgMinWidth),
dialogs(this), history(this), profile(0), overview(0), _topBar(this), _forwardConfirm(0), hider(0), _mediaType(this), _mediaTypeMask(0),
updGoodPts(0), updLastPts(0), updPtsCount(0), updDate(0), updQts(-1), updSeq(0), updInited(false), updSkipPtsUpdateLevel(0), _onlineRequest(0), _lastWasOnline(false), _lastSetOnline(0), _isIdle(false),
_failDifferenceTimeout(1), _lastUpdateTime(0), _cachedX(0), _cachedY(0), _background(0), _api(new ApiWrap(this)) {
	setGeometry(QRect(0, st::titleHeight, App::wnd()->width(), App::wnd()->height() - st::titleHeight));

	updateScrollColors();

	connect(window, SIGNAL(resized(const QSize &)), this, SLOT(onParentResize(const QSize &)));
	connect(&dialogs, SIGNAL(cancelled()), this, SLOT(dialogsCancelled()));
	connect(&history, SIGNAL(cancelled()), &dialogs, SLOT(activate()));
	connect(this, SIGNAL(peerPhotoChanged(PeerData*)), this, SIGNAL(dialogsUpdated()));
	connect(&noUpdatesTimer, SIGNAL(timeout()), this, SLOT(mtpPing()));
	connect(&_onlineTimer, SIGNAL(timeout()), this, SLOT(updateOnline()));
	connect(&_onlineUpdater, SIGNAL(timeout()), this, SLOT(updateOnlineDisplay()));
	connect(&_idleFinishTimer, SIGNAL(timeout()), this, SLOT(checkIdleFinish()));
	connect(&_bySeqTimer, SIGNAL(timeout()), this, SLOT(getDifference()));
	connect(&_byPtsTimer, SIGNAL(timeout()), this, SLOT(getDifference()));
	connect(&_failDifferenceTimer, SIGNAL(timeout()), this, SLOT(getDifferenceForce()));
	connect(this, SIGNAL(peerUpdated(PeerData*)), &history, SLOT(peerUpdated(PeerData*)));
	connect(&_topBar, SIGNAL(clicked()), this, SLOT(onTopBarClick()));
	connect(&history, SIGNAL(peerShown(PeerData*)), this, SLOT(onPeerShown(PeerData*)));
	connect(&updateNotifySettingTimer, SIGNAL(timeout()), this, SLOT(onUpdateNotifySettings()));
	connect(this, SIGNAL(showPeerAsync(quint64,qint32,bool,bool)), this, SLOT(showPeer(quint64,qint32,bool,bool)), Qt::QueuedConnection);
	if (audioVoice()) {
		connect(audioVoice(), SIGNAL(updated(AudioData*)), this, SLOT(audioPlayProgress(AudioData*)));
		connect(audioVoice(), SIGNAL(stopped(AudioData*)), this, SLOT(audioPlayProgress(AudioData*)));
	}

	_webPageUpdater.setSingleShot(true);
	connect(&_webPageUpdater, SIGNAL(timeout()), this, SLOT(webPagesUpdate()));

	connect(&_cacheBackgroundTimer, SIGNAL(timeout()), this, SLOT(onCacheBackground()));

	dialogs.show();
	if (cWideMode()) {
        history.show();
	} else {
		history.hide();
    }
	App::wnd()->getTitle()->updateBackButton();
	_topBar.hide();

	_topBar.raise();
    dialogs.raise();
	_mediaType.raise();

	MTP::setGlobalFailHandler(rpcFail(&MainWidget::updateFail));

	_mediaType.hide();
	_topBar.mediaTypeButton()->installEventFilter(&_mediaType);

	show();
	setFocus();

    _api->init();
}

void MainWidget::onForward(const PeerId &peer, bool forwardSelected) {
	history.cancelReply();
	_toForward.clear();
	if (forwardSelected) {
		if (overview) {
			overview->fillSelectedItems(_toForward, false);
		} else {
			history.fillSelectedItems(_toForward, false);
		}
	} else if (App::contextItem() && dynamic_cast<HistoryMessage*>(App::contextItem()) && App::contextItem()->id > 0) {
		_toForward.insert(App::contextItem()->id, App::contextItem());
	}
	updateForwardingTexts();
	showPeer(peer, 0, false, true);
	history.onClearSelected();
	history.updateForwarding();
}

bool MainWidget::hasForwardingItems() {
	return !_toForward.isEmpty();
}

void MainWidget::fillForwardingInfo(Text *&from, Text *&text, bool &serviceColor, ImagePtr &preview) {
	if (_toForward.isEmpty()) return;
	int32 version = 0;
	for (SelectedItemSet::const_iterator i = _toForward.cbegin(), e = _toForward.cend(); i != e; ++i) {
		version += i.value()->from()->nameVersion;
	}
	if (version != _toForwardNameVersion) {
		updateForwardingTexts();
	}
	from = &_toForwardFrom;
	text = &_toForwardText;
	serviceColor = (_toForward.size() > 1) || _toForward.cbegin().value()->getMedia() || _toForward.cbegin().value()->serviceMsg();
	if (_toForward.size() < 2 && _toForward.cbegin().value()->getMedia() && _toForward.cbegin().value()->getMedia()->hasReplyPreview()) {
		preview = _toForward.cbegin().value()->getMedia()->replyPreview();
	}
}

void MainWidget::updateForwardingTexts() {
	int32 version = 0;
	QString from, text;
	if (!_toForward.isEmpty()) {
		QMap<UserData*, bool> fromUsersMap;
		QVector<UserData*> fromUsers;
		fromUsers.reserve(_toForward.size());
		for (SelectedItemSet::const_iterator i = _toForward.cbegin(), e = _toForward.cend(); i != e; ++i) {
			if (!fromUsersMap.contains(i.value()->from())) {
				fromUsersMap.insert(i.value()->from(), true);
				fromUsers.push_back(i.value()->from());
			}
			version += i.value()->from()->nameVersion;
		}
		if (fromUsers.size() > 2) {
			from = lng_forwarding_from(lt_user, fromUsers.at(0)->firstName, lt_count, fromUsers.size() - 1);
		} else if (fromUsers.size() < 2) {
			from = fromUsers.at(0)->name;
		} else {
			from = lng_forwarding_from_two(lt_user, fromUsers.at(0)->firstName, lt_second_user, fromUsers.at(1)->firstName);
		}

		if (_toForward.size() < 2) {
			text = _toForward.cbegin().value()->inReplyText();
		} else {
			text = lng_forward_messages(lt_count, _toForward.size());
		}
	}
	_toForwardFrom.setText(st::msgServiceNameFont, from, _textNameOptions);
	_toForwardText.setText(st::msgFont, text, _textDlgOptions);
	_toForwardNameVersion = version;
}

void MainWidget::cancelForwarding() {
	if (_toForward.isEmpty()) return;

	_toForward.clear();
	history.cancelForwarding();
}

void MainWidget::finishForwarding(History *hist) {
	if (!hist) return;
	if (!_toForward.isEmpty()) {
		App::main()->readServerHistory(hist, false);
		if (_toForward.size() < 2) {
			uint64 randomId = MTP::nonce<uint64>();
			MsgId newId = clientMsgId();
			hist->addToBackForwarded(newId, static_cast<HistoryMessage*>(_toForward.cbegin().value()));
			App::historyRegRandom(randomId, newId);
			hist->sendRequestId = MTP::send(MTPmessages_ForwardMessage(hist->peer->input, MTP_int(_toForward.cbegin().key()), MTP_long(randomId)), rpcDone(&MainWidget::sentUpdatesReceived), RPCFailHandlerPtr(), 0, 0, hist->sendRequestId);
		} else {
			QVector<MTPint> ids;
			QVector<MTPlong> randomIds;
			ids.reserve(_toForward.size());
			randomIds.reserve(_toForward.size());
			for (SelectedItemSet::const_iterator i = _toForward.cbegin(), e = _toForward.cend(); i != e; ++i) {
				uint64 randomId = MTP::nonce<uint64>();
				//MsgId newId = clientMsgId();
				//hist->addToBackForwarded(newId, static_cast<HistoryMessage*>(i.value()));
				//App::historyRegRandom(randomId, newId);
				ids.push_back(MTP_int(i.key()));
				randomIds.push_back(MTP_long(randomId));
			}
			hist->sendRequestId = MTP::send(MTPmessages_ForwardMessages(hist->peer->input, MTP_vector<MTPint>(ids), MTP_vector<MTPlong>(randomIds)), rpcDone(&MainWidget::sentUpdatesReceived), RPCFailHandlerPtr(), 0, 0, hist->sendRequestId);
		}
		if (history.peer() == hist->peer) history.peerMessagesUpdated();
		cancelForwarding();
	}

	historyToDown(hist);
	dialogsToUp();
	history.peerMessagesUpdated(hist->peer->id);
}

void MainWidget::webPageUpdated(WebPageData *data) {
	_webPagesUpdated.insert(data->id, true);
	_webPageUpdater.start(0);
}

void MainWidget::webPagesUpdate() {
	const WebPageItems &items(App::webPageItems());
	for (QMap<WebPageId, bool>::const_iterator i = _webPagesUpdated.cbegin(), e = _webPagesUpdated.cend(); i != e; ++i) {
		WebPageItems::const_iterator j = items.constFind(App::webPage(i.key()));
		if (j != items.cend()) {
			for (HistoryItemsMap::const_iterator k = j.value().cbegin(), e = j.value().cend(); k != e; ++k) {
				k.key()->initDimensions();
				itemResized(k.key());
			}
		}
	}
	_webPagesUpdated.clear();
}

void MainWidget::onShareContact(const PeerId &peer, UserData *contact) {
	history.onShareContact(peer, contact);
}

void MainWidget::onSendPaths(const PeerId &peer) {
	history.onSendPaths(peer);
}

void MainWidget::noHider(HistoryHider *destroyed) {
	if (hider == destroyed) {
		hider = 0;
		if (cWideMode()) {
			if (_forwardConfirm) {
				_forwardConfirm->deleteLater();
				_forwardConfirm = 0;
			}
		} else {
			if (_forwardConfirm) {
				_forwardConfirm->startHide();
				_forwardConfirm = 0;
			}
			onPeerShown(history.peer());
			if (profile || overview || (history.peer() && history.peer()->id)) {
				dialogs.enableShadow(false);
				QPixmap animCache = myGrab(this, QRect(0, st::topBarHeight, _dialogsWidth, height() - st::topBarHeight)),
					animTopBarCache = myGrab(this, QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight));
				dialogs.enableShadow();
				_topBar.enableShadow();
				dialogs.hide();
				if (overview) {
					overview->show();
					overview->animShow(animCache, animTopBarCache);
				} else if (profile) {
					profile->show();
					profile->animShow(animCache, animTopBarCache);
				} else {
					history.show();
					history.animShow(animCache, animTopBarCache);
				}
			}
			App::wnd()->getTitle()->updateBackButton();
		}
	}
}

void MainWidget::hiderLayer(HistoryHider *h) {
	if (App::passcoded()) {
		delete h;
		return;
	}

	hider = h;
	connect(hider, SIGNAL(forwarded()), &dialogs, SLOT(onCancelSearch()));
	dialogsToUp();
	if (cWideMode()) {
		hider->show();
		resizeEvent(0);
		dialogs.activate();
	} else {
		hider->hide();
		dialogs.enableShadow(false);
		QPixmap animCache = myGrab(this, QRect(0, 0, _dialogsWidth, height()));
		dialogs.enableShadow();
		_topBar.enableShadow();

		onPeerShown(0);
		if (overview) {
			overview->hide();
		} else if (profile) {
			profile->hide();
		} else {
			history.hide();
		}
		dialogs.show();
		resizeEvent(0);
		dialogs.animShow(animCache);
		App::wnd()->getTitle()->updateBackButton();
	}
}

void MainWidget::forwardLayer(int32 forwardSelected) {
	hiderLayer((forwardSelected < 0) ? (new HistoryHider(this)) : (new HistoryHider(this, forwardSelected > 0)));
}

void MainWidget::deleteLayer(int32 selectedCount) {
	QString str((selectedCount < 0) ? lang(selectedCount < -1 ? lng_selected_cancel_sure_this : lng_selected_delete_sure_this) : lng_selected_delete_sure(lt_count, selectedCount));
	ConfirmBox *box = new ConfirmBox((selectedCount < 0) ? str : str.arg(selectedCount), lang(lng_selected_delete_confirm));
	if (selectedCount < 0) {
		connect(box, SIGNAL(confirmed()), overview ? overview : static_cast<QWidget*>(&history), SLOT(onDeleteContextSure()));
	} else {
		connect(box, SIGNAL(confirmed()), overview ? overview : static_cast<QWidget*>(&history), SLOT(onDeleteSelectedSure()));
	}
	App::wnd()->showLayer(box);
}

void MainWidget::shareContactLayer(UserData *contact) {
	hiderLayer(new HistoryHider(this, contact));
}

bool MainWidget::selectingPeer(bool withConfirm) {
	return hider ? (withConfirm ? hider->withConfirm() : true) : false;
}

void MainWidget::offerPeer(PeerId peer) {
	if (hider->offerPeer(peer) && !cWideMode()) {
		_forwardConfirm = new ConfirmBox(hider->offeredText(), lang(lng_forward));
		connect(_forwardConfirm, SIGNAL(confirmed()), hider, SLOT(forward()));
		connect(_forwardConfirm, SIGNAL(cancelled()), this, SLOT(onForwardCancel()));
		connect(_forwardConfirm, SIGNAL(destroyed(QObject*)), this, SLOT(onForwardCancel(QObject*)));
		App::wnd()->showLayer(_forwardConfirm);
	}
}

void MainWidget::onForwardCancel(QObject *obj) {
	if (!obj || obj == _forwardConfirm) {
		if (_forwardConfirm) {
			if (!obj) _forwardConfirm->startHide();
			_forwardConfirm = 0;
		}
		if (hider) hider->offerPeer(0);
	}
}

void MainWidget::focusPeerSelect() {
	hider->setFocus();
}

void MainWidget::dialogsActivate() {
	dialogs.activate();
}

bool MainWidget::leaveChatFailed(PeerData *peer, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	if (error.type() == "CHAT_ID_INVALID") { // left this chat already
		if ((profile && profile->peer() == peer) || (overview && overview->peer() == peer) || _stack.contains(peer) || history.peer() == peer) {
			showPeer(0, 0, false, true);
		}
		dialogs.removePeer(peer);
		App::histories().remove(peer->id);
		MTP::send(MTPmessages_DeleteHistory(peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, peer));
		return true;
	}
	return false;
}

void MainWidget::deleteHistory(PeerData *peer, const MTPUpdates &updates) {
	sentUpdatesReceived(updates);
	if ((profile && profile->peer() == peer) || (overview && overview->peer() == peer) || _stack.contains(peer) || history.peer() == peer) {
		showPeer(0, 0, false, true);
	}
	dialogs.removePeer(peer);
	App::histories().remove(peer->id);
	MTP::send(MTPmessages_DeleteHistory(peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, peer));
}

void MainWidget::deleteHistoryPart(PeerData *peer, const MTPmessages_AffectedHistory &result) {
	const MTPDmessages_affectedHistory &d(result.c_messages_affectedHistory());
	updPtsUpdated(d.vpts.v, d.vpts_count.v);

	int32 offset = d.voffset.v;
	if (!MTP::authedId() || offset <= 0) return;

	MTP::send(MTPmessages_DeleteHistory(peer->input, d.voffset), rpcDone(&MainWidget::deleteHistoryPart, peer));
}

void MainWidget::deleteMessages(const QVector<MTPint> &ids) {
	MTP::send(MTPmessages_DeleteMessages(MTP_vector<MTPint>(ids)), rpcDone(&MainWidget::msgsWereDeleted));
}

void MainWidget::deletedContact(UserData *user, const MTPcontacts_Link &result) {
	const MTPDcontacts_link &d(result.c_contacts_link());
	App::feedUsers(MTP_vector<MTPUser>(1, d.vuser));
	App::feedUserLink(MTP_int(user->id & 0xFFFFFFFF), d.vmy_link, d.vforeign_link);
}

void MainWidget::deleteHistoryAndContact(UserData *user, const MTPcontacts_Link &result) {
	const MTPDcontacts_link &d(result.c_contacts_link());
	App::feedUsers(MTP_vector<MTPUser>(1, d.vuser));
	App::feedUserLink(MTP_int(user->id & 0xFFFFFFFF), d.vmy_link, d.vforeign_link);

	if ((profile && profile->peer() == user) || (overview && overview->peer() == user) || _stack.contains(user) || history.peer() == user) {
		showPeer(0);
	}
	dialogs.removePeer(user);
	MTP::send(MTPmessages_DeleteHistory(user->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, (PeerData*)user));
}

void MainWidget::clearHistory(PeerData *peer) {
	if (!peer->chat && peer->asUser()->contact <= 0) {
		dialogs.removePeer(peer->asUser());
	}
	dialogsToUp();
	dialogs.update();
	App::history(peer->id)->clear();
	MTP::send(MTPmessages_DeleteHistory(peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, peer));
}

void MainWidget::removeContact(UserData *user) {
	dialogs.removeContact(user);
}

void MainWidget::addParticipants(ChatData *chat, const QVector<UserData*> &users) {
	for (QVector<UserData*>::const_iterator i = users.cbegin(), e = users.cend(); i != e; ++i) {
		MTP::send(MTPmessages_AddChatUser(MTP_int(chat->id & 0xFFFFFFFF), (*i)->inputUser, MTP_int(ForwardOnAdd)), rpcDone(&MainWidget::sentUpdatesReceived), rpcFail(&MainWidget::addParticipantFail, chat), 0, 5);
	}
	App::wnd()->hideLayer();
	showPeer(chat->id, 0, false);
}

bool MainWidget::addParticipantFail(ChatData *chat, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	ConfirmBox *box = new ConfirmBox(lang(lng_failed_add_participant), true);
	App::wnd()->showLayer(box);
	if (error.type() == "USER_LEFT_CHAT") { // trying to return banned user to his group
	}
	return false;
}

void MainWidget::kickParticipant(ChatData *chat, UserData *user) {
	MTP::send(MTPmessages_DeleteChatUser(MTP_int(chat->id & 0xFFFFFFFF), user->inputUser), rpcDone(&MainWidget::sentUpdatesReceived), rpcFail(&MainWidget::kickParticipantFail, chat));
	App::wnd()->hideLayer();
	showPeer(chat->id, 0, false);
}

bool MainWidget::kickParticipantFail(ChatData *chat, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	error.type();
	return false;
}

void MainWidget::checkPeerHistory(PeerData *peer) {
	MTP::send(MTPmessages_GetHistory(peer->input, MTP_int(0), MTP_int(0), MTP_int(1)), rpcDone(&MainWidget::checkedHistory, peer));
}

void MainWidget::checkedHistory(PeerData *peer, const MTPmessages_Messages &result) {
	const QVector<MTPMessage> *v = 0;
	if (result.type() == mtpc_messages_messages) {
		const MTPDmessages_messages &d(result.c_messages_messages());
		App::feedChats(d.vchats);
		App::feedUsers(d.vusers);
		v = &d.vmessages.c_vector().v;
	} else if (result.type() == mtpc_messages_messagesSlice) {
		const MTPDmessages_messagesSlice &d(result.c_messages_messagesSlice());
		App::feedChats(d.vchats);
		App::feedUsers(d.vusers);
		v = &d.vmessages.c_vector().v;
	}
	if (!v) return;

	if (v->isEmpty()) {
		if ((profile && profile->peer() == peer) || (overview && overview->peer() == peer) || _stack.contains(peer) || history.peer() == peer) {
			showPeer(0);
		}
		dialogs.removePeer(peer);
	} else {
		History *h = App::historyLoaded(peer->id);
		if (!h->last) {
			h->addToBack((*v)[0], 0);
		}
	}
}

bool MainWidget::sendPhotoFailed(uint64 randomId, const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	if (error.type() == qsl("PHOTO_INVALID_DIMENSIONS")) {
		if (_resendImgRandomIds.isEmpty()) {
			ConfirmBox *box = new ConfirmBox(lang(lng_bad_image_for_photo));
			connect(box, SIGNAL(confirmed()), this, SLOT(onResendAsDocument()));
			connect(box, SIGNAL(cancelled()), this, SLOT(onCancelResend()));
			connect(box, SIGNAL(destroyed(QObject*)), this, SLOT(onCancelResend()));
			App::wnd()->showLayer(box);
		}
		_resendImgRandomIds.push_back(randomId);
		return true;
	}
	return false;
}

void MainWidget::onResendAsDocument() {
	QList<uint64> tmp = _resendImgRandomIds;
	_resendImgRandomIds.clear();
	for (int32 i = 0, l = tmp.size(); i < l; ++i) {
		if (HistoryItem *item = App::histItemById(App::histItemByRandom(tmp.at(i)))) {
			if (HistoryPhoto *media = dynamic_cast<HistoryPhoto*>(item->getMedia())) {
				PhotoData *photo = media->photo();
				if (!photo->full->isNull()) {
					photo->full->forget();
					QByteArray data = photo->full->savedData();
					if (!data.isEmpty()) {
						history.uploadMedia(data, ToPrepareDocument, item->history()->peer->id);
					}
				}
			}
			item->destroy();
		}
	}
	App::wnd()->layerHidden();
}

void MainWidget::onCancelResend() {
	QList<uint64> tmp = _resendImgRandomIds;
	_resendImgRandomIds.clear();
	for (int32 i = 0, l = tmp.size(); i < l; ++i) {
		if (HistoryItem *item = App::histItemById(App::histItemByRandom(tmp.at(i)))) {
			item->destroy();
		}
	}
}

void MainWidget::onCacheBackground() {
	const QPixmap &bg(*cChatBackground());
	if (cTileBackground()) {
		QImage result(_willCacheFor.width() * cIntRetinaFactor(), _willCacheFor.height() * cIntRetinaFactor(), QImage::Format_RGB32);
        result.setDevicePixelRatio(cRetinaFactor());
		{
			QPainter p(&result);
			int left = 0, top = 0, right = _willCacheFor.width(), bottom = _willCacheFor.height();
			float64 w = bg.width() / cRetinaFactor(), h = bg.height() / cRetinaFactor();
			int sx = 0, sy = 0, cx = qCeil(_willCacheFor.width() / w), cy = qCeil(_willCacheFor.height() / h);
			for (int i = sx; i < cx; ++i) {
				for (int j = sy; j < cy; ++j) {
					p.drawPixmap(QPointF(i * w, j * h), bg);
				}
			}
		}
		_cachedX = 0;
		_cachedY = 0;
		_cachedBackground = QPixmap::fromImage(result);
	} else {
		QRect to, from;
		backgroundParams(_willCacheFor, to, from);
		_cachedX = to.x();
		_cachedY = to.y();
		_cachedBackground = QPixmap::fromImage(bg.toImage().copy(from).scaled(to.width() * cIntRetinaFactor(), to.height() * cIntRetinaFactor(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		_cachedBackground.setDevicePixelRatio(cRetinaFactor());
	}
	_cachedFor = _willCacheFor;
}

void MainWidget::forwardSelectedItems() {
	if (overview) {
		overview->onForwardSelected();
	} else {
		history.onForwardSelected();
	}
}

void MainWidget::deleteSelectedItems() {
	if (overview) {
		overview->onDeleteSelected();
	} else {
		history.onDeleteSelected();
	}
}

void MainWidget::clearSelectedItems() {
	if (overview) {
		overview->onClearSelected();
	} else {
		history.onClearSelected();
	}
}

DialogsIndexed &MainWidget::contactsList() {
	return dialogs.contactsList();
}

void MainWidget::sendPreparedText(History *hist, const QString &text, MsgId replyTo, WebPageId webPageId) {
	saveRecentHashtags(text);
	QString sendingText, leftText = text;
	if (replyTo < 0) replyTo = history.replyToId();
	while (textSplit(sendingText, leftText, MaxMessageSize)) {
		MsgId newId = clientMsgId();
		uint64 randomId = MTP::nonce<uint64>();

		App::historyRegRandom(randomId, newId);

		MTPstring msgText(MTP_string(sendingText));
		int32 flags = (hist->peer->input.type() == mtpc_inputPeerSelf) ? 0 : (MTPDmessage_flag_unread | MTPDmessage_flag_out);
		int32 sendFlags = 0;
		if (replyTo) {
			flags |= MTPDmessage::flag_reply_to_msg_id;
			sendFlags |= MTPmessages_SendMessage::flag_reply_to_msg_id;
		}
		MTPMessageMedia media = MTP_messageMediaEmpty();
		if (webPageId == 0xFFFFFFFFFFFFFFFFULL) {
			sendFlags |= MTPmessages_SendMessage_flag_skipWebPage;
		} else if (webPageId) {
			WebPageData *page = App::webPage(webPageId);
			media = MTP_messageMediaWebPage(MTP_webPagePending(MTP_long(page->id), MTP_int(page->pendingTill)));
		}
		hist->addToBack(MTP_message(MTP_int(flags), MTP_int(newId), MTP_int(MTP::authedId()), App::peerToMTP(hist->peer->id), MTPint(), MTPint(), MTP_int(replyTo), MTP_int(unixtime()), msgText, media));
		hist->sendRequestId = MTP::send(MTPmessages_SendMessage(MTP_int(sendFlags), hist->peer->input, MTP_int(replyTo), msgText, MTP_long(randomId)), App::main()->rpcDone(&MainWidget::sentDataReceived, randomId), RPCFailHandlerPtr(), 0, 0, hist->sendRequestId);
	}

	finishForwarding(hist);
}

void MainWidget::sendMessage(History *hist, const QString &text, MsgId replyTo) {
	readServerHistory(hist, false);
	hist->loadAround(0);
	sendPreparedText(hist, history.prepareMessage(text), replyTo);
}

void MainWidget::saveRecentHashtags(const QString &text) {
	bool found = false;
	QRegularExpressionMatch m;
	RecentHashtagPack recent(cRecentWriteHashtags());
	for (int32 i = 0, next = 0; (m = reHashtag().match(text, i)).hasMatch(); i = next) {
		i = m.capturedStart();
		next = m.capturedEnd();
		if (m.hasMatch()) {
			if (!m.capturedRef(1).isEmpty()) {
				++i;
			}
			if (!m.capturedRef(2).isEmpty()) {
				--next;
			}
		}
		if (!found && cRecentWriteHashtags().isEmpty() && cRecentSearchHashtags().isEmpty()) {
			Local::readRecentStickers();
			recent = cRecentWriteHashtags();
		}
		found = true;
		incrementRecentHashtag(recent, text.mid(i + 1, next - i - 1));
	}
	if (found) {
		cSetRecentWriteHashtags(recent);
		Local::writeRecentHashtags();
	}
}

void MainWidget::readServerHistory(History *hist, bool force) {
	if (!hist || (!force && (!hist->unreadCount || !hist->readyForWork()))) return;
    
    ReadRequests::const_iterator i = _readRequests.constFind(hist->peer);
    if (i == _readRequests.cend()) {
        hist->inboxRead(0);
        _readRequests.insert(hist->peer, MTP::send(MTPmessages_ReadHistory(hist->peer->input, MTP_int(0), MTP_int(0)), rpcDone(&MainWidget::partWasRead, hist->peer)));
    }
}

uint64 MainWidget::animActiveTime() const {
	return history.animActiveTime();
}

void MainWidget::stopAnimActive() {
	history.stopAnimActive();
}

void MainWidget::searchMessages(const QString &query) {
	dialogs.searchMessages(query);
	if (!cWideMode()) onShowDialogs();
}

void MainWidget::preloadOverviews(PeerData *peer) {
	History *h = App::history(peer->id);
	bool sending[OverviewCount] = { false };
	for (int32 i = 0; i < OverviewCount; ++i) {
		if (h->_overviewCount[i] < 0) {
			if (_overviewPreload[i].constFind(peer) == _overviewPreload[i].cend()) {
				sending[i] = true;
			}
		}
	}
	int32 last = OverviewCount;
	while (last > 0) {
		if (sending[--last]) break;
	}
	for (int32 i = 0; i < OverviewCount; ++i) {
		if (sending[i]) {
			MediaOverviewType type = MediaOverviewType(i);
			MTPMessagesFilter filter = typeToMediaFilter(type);
			if (type == OverviewCount) break;

			_overviewPreload[i].insert(peer, MTP::send(MTPmessages_Search(peer->input, MTP_string(""), filter, MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(0)), rpcDone(&MainWidget::overviewPreloaded, peer), rpcFail(&MainWidget::overviewFailed, peer), 0, (i == last) ? 0 : 10));
		}
	}
}

void MainWidget::overviewPreloaded(PeerData *peer, const MTPmessages_Messages &result, mtpRequestId req) {
	MediaOverviewType type = OverviewCount;
	for (int32 i = 0; i < OverviewCount; ++i) {
		OverviewsPreload::iterator j = _overviewPreload[i].find(peer);
		if (j != _overviewPreload[i].end() && j.value() == req) {
			type = MediaOverviewType(i);
			_overviewPreload[i].erase(j);
			break;
		}
	}

	if (type == OverviewCount) return;

	History *h = App::history(peer->id);
	switch (result.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(result.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vmessages.c_vector().v.size();
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(result.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vcount.v;
	} break;

	default: return;
	}

	if (h->_overviewCount[type] > 0) {
		for (History::MediaOverviewIds::const_iterator i = h->_overviewIds[type].cbegin(), e = h->_overviewIds[type].cend(); i != e; ++i) {
			if (i.key() < 0) {
				++h->_overviewCount[type];
			} else {
				break;
			}
		}
	}

	mediaOverviewUpdated(peer);
}

void MainWidget::mediaOverviewUpdated(PeerData *peer) {
	if (profile) profile->mediaOverviewUpdated(peer);
	if (overview && overview->peer() == peer) {
		overview->mediaOverviewUpdated(peer);

		int32 mask = 0;
		History *h = peer ? App::historyLoaded(peer->id) : 0;
		if (h) {
			for (int32 i = 0; i < OverviewCount; ++i) {
				if (!h->_overview[i].isEmpty() || h->_overviewCount[i] > 0 || i == overview->type()) {
					mask |= (1 << i);
				}
			}
		}
		if (mask != _mediaTypeMask) {
			_mediaType.resetButtons();
			for (int32 i = 0; i < OverviewCount; ++i) {
				if (mask & (1 << i)) {
					switch (i) {
					case OverviewPhotos: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaPhotos, lang(lng_media_type_photos))), SIGNAL(clicked()), this, SLOT(onPhotosSelect())); break;
					case OverviewVideos: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaVideos, lang(lng_media_type_videos))), SIGNAL(clicked()), this, SLOT(onVideosSelect())); break;
					case OverviewDocuments: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaDocuments, lang(lng_media_type_files))), SIGNAL(clicked()), this, SLOT(onDocumentsSelect())); break;
					case OverviewAudios: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaAudios, lang(lng_media_type_audios))), SIGNAL(clicked()), this, SLOT(onAudiosSelect())); break;
					}
				}
			}
			_mediaTypeMask = mask;
			_mediaType.move(width() - _mediaType.width(), st::topBarHeight);
			overview->updateTopBarSelection();
		}
	}
}

void MainWidget::changingMsgId(HistoryItem *row, MsgId newId) {
	if (overview) overview->changingMsgId(row, newId);
}

void MainWidget::itemRemoved(HistoryItem *item) {
	api()->itemRemoved(item);
	dialogs.itemRemoved(item);
	if (history.peer() == item->history()->peer) {
		history.itemRemoved(item);
	}
	itemRemovedGif(item);
	if (!_toForward.isEmpty()) {
		SelectedItemSet::iterator i = _toForward.find(item->id);
		if (i != _toForward.end()) {
			_toForward.erase(i);
			updateForwardingTexts();
		}
	}
}

void MainWidget::itemReplaced(HistoryItem *oldItem, HistoryItem *newItem) {
	api()->itemReplaced(oldItem, newItem);
	dialogs.itemReplaced(oldItem, newItem);
	if (history.peer() == newItem->history()->peer) {
		history.itemReplaced(oldItem, newItem);
	}
	itemReplacedGif(oldItem, newItem);
	if (!_toForward.isEmpty()) {
		SelectedItemSet::iterator i = _toForward.find(oldItem->id);
		if (i != _toForward.end()) {
			i.value() = newItem;
		}
	}
}

void MainWidget::itemResized(HistoryItem *row, bool scrollToIt) {
	if (!row || (history.peer() == row->history()->peer && !row->detached())) {
		history.itemResized(row, scrollToIt);
	} else if (row) {
		row->history()->width = 0;
		if (history.peer() == row->history()->peer) {
			history.resizeEvent(0);
		}
	}
	if (overview) {
		overview->itemResized(row, scrollToIt);
	}
}

bool MainWidget::overviewFailed(PeerData *peer, const RPCError &error, mtpRequestId req) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	MediaOverviewType type = OverviewCount;
	for (int32 i = 0; i < OverviewCount; ++i) {
		OverviewsPreload::iterator j = _overviewPreload[i].find(peer);
		if (j != _overviewPreload[i].end() && j.value() == req) {
			_overviewPreload[i].erase(j);
			break;
		}
	}
	return true;
}

void MainWidget::loadMediaBack(PeerData *peer, MediaOverviewType type, bool many) {
	if (_overviewLoad[type].constFind(peer) != _overviewLoad[type].cend()) return;

	MsgId minId = 0;
	History *hist = App::history(peer->id);
	if (hist->_overviewCount[type] == 0) return; // all loaded

	for (History::MediaOverviewIds::const_iterator i = hist->_overviewIds[type].cbegin(), e = hist->_overviewIds[type].cend(); i != e; ++i) {
		if (i.key() > 0) {
			minId = i.key();
			break;
		}
	}
	int32 limit = many ? SearchManyPerPage : (hist->_overview[type].size() > MediaOverviewStartPerPage) ? SearchPerPage : MediaOverviewStartPerPage;
	MTPMessagesFilter filter = typeToMediaFilter(type);
	if (type == OverviewCount) return;

	_overviewLoad[type].insert(hist->peer, MTP::send(MTPmessages_Search(hist->peer->input, MTPstring(), filter, MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(minId), MTP_int(limit)), rpcDone(&MainWidget::photosLoaded, hist)));
}

void MainWidget::peerUsernameChanged(PeerData *peer) {
	if (profile && profile->peer() == peer) {
		profile->update();
	}
	if (App::settings() && peer == App::self()) {
		App::settings()->usernameChanged();
	}
}

void MainWidget::checkLastUpdate(bool afterSleep) {
	uint64 n = getms(true);
	if (_lastUpdateTime && n > _lastUpdateTime + (afterSleep ? NoUpdatesAfterSleepTimeout : NoUpdatesTimeout)) {
		_lastUpdateTime = n;
		MTP::ping();
	}
}

void MainWidget::showAddContact() {
	dialogs.onAddContact();
}

void MainWidget::showNewGroup() {
	dialogs.onNewGroup();
}

void MainWidget::photosLoaded(History *h, const MTPmessages_Messages &msgs, mtpRequestId req) {
	OverviewsPreload::iterator it;
	MediaOverviewType type = OverviewCount;
	for (int32 i = 0; i < OverviewCount; ++i) {
		it = _overviewLoad[i].find(h->peer);
		if (it != _overviewLoad[i].cend()) {
			type = MediaOverviewType(i);
			_overviewLoad[i].erase(it);
			break;
		}
	}
	if (type == OverviewCount) return;

	const QVector<MTPMessage> *v = 0;
	switch (msgs.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(msgs.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
		h->_overviewCount[type] = 0;
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(msgs.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vcount.v;
		v = &d.vmessages.c_vector().v;
	} break;

	default: return;
	}

	if (h->_overviewCount[type] > 0) {
		for (History::MediaOverviewIds::const_iterator i = h->_overviewIds[type].cbegin(), e = h->_overviewIds[type].cend(); i != e; ++i) {
			if (i.key() < 0) {
				++h->_overviewCount[type];
			} else {
				break;
			}
		}
	}
	if (v->isEmpty()) {
		h->_overviewCount[type] = 0;
	}

	for (QVector<MTPMessage>::const_iterator i = v->cbegin(), e = v->cend(); i != e; ++i) {
		HistoryItem *item = App::histories().addToBack(*i, -1);
		if (item && h->_overviewIds[type].constFind(item->id) == h->_overviewIds[type].cend()) {
			h->_overviewIds[type].insert(item->id, NullType());
			h->_overview[type].push_front(item->id);
		}
	}
	if (App::wnd()) App::wnd()->mediaOverviewUpdated(h->peer);
}

void MainWidget::partWasRead(PeerData *peer, const MTPmessages_AffectedHistory &result) {
	const MTPDmessages_affectedHistory &d(result.c_messages_affectedHistory());
	updPtsUpdated(d.vpts.v, d.vpts_count.v);
    
	int32 offset = d.voffset.v;
	if (!MTP::authedId() || offset <= 0) {
        _readRequests.remove(peer);
    } else {
        _readRequests[peer] = MTP::send(MTPmessages_ReadHistory(peer->input, MTP_int(0), MTP_int(offset)), rpcDone(&MainWidget::partWasRead, peer));
    }
}

void MainWidget::msgsWereDeleted(const MTPmessages_AffectedMessages &result) {
	const MTPDmessages_affectedMessages &d(result.c_messages_affectedMessages());
	updPtsUpdated(d.vpts.v, d.vpts_count.v);
}

void MainWidget::videoLoadProgress(mtpFileLoader *loader) {
	VideoData *video = App::video(loader->objId());
	if (video->loader) {
		if (video->loader->done()) {
			video->finish();
			QString already = video->already();
			if (!already.isEmpty() && video->openOnSave) {
				QPoint pos(QCursor::pos());
				if (video->openOnSave < 0 && !psShowOpenWithMenu(pos.x(), pos.y(), already)) {
					psOpenFile(already, true);
				} else {
					psOpenFile(already, video->openOnSave < 0);
				}
			}
		}
	}
	const VideoItems &items(App::videoItems());
	VideoItems::const_iterator i = items.constFind(video);
	if (i != items.cend()) {
		for (HistoryItemsMap::const_iterator j = i->cbegin(), e = i->cend(); j != e; ++j) {
			msgUpdated(j.key()->history()->peer->id, j.key());
		}
	}
}

void MainWidget::loadFailed(mtpFileLoader *loader, bool started, const char *retrySlot) {
	failedObjId = loader->objId();
	failedFileName = loader->fileName();
	ConfirmBox *box = new ConfirmBox(lang(started ? lng_download_finish_failed : lng_download_path_failed), started ? QString() : lang(lng_download_path_settings));
	if (started) {
		connect(box, SIGNAL(confirmed()), this, retrySlot);
	} else {
		connect(box, SIGNAL(confirmed()), App::wnd(), SLOT(showSettings()));
	}
	App::wnd()->showLayer(box);
}

void MainWidget::videoLoadFailed(mtpFileLoader *loader, bool started) {
	loadFailed(loader, started, SLOT(videoLoadRetry()));
	VideoData *video = App::video(loader->objId());
	if (video && video->loader) video->finish();
}

void MainWidget::videoLoadRetry() {
	App::wnd()->hideLayer();
	VideoData *video = App::video(failedObjId);
	if (video) video->save(failedFileName);
}

void MainWidget::audioLoadProgress(mtpFileLoader *loader) {
	AudioData *audio = App::audio(loader->objId());
	if (audio->loader) {
		if (audio->loader->done()) {
			audio->finish();
			bool mp3 = (audio->mime == QLatin1String("audio/mp3"));
			QString already = audio->already();
			bool play = !mp3 && audio->openOnSave > 0 && audioVoice();
			if ((!already.isEmpty() && audio->openOnSave) || (!audio->data.isEmpty() && play)) {
				if (play) {
					AudioData *playing = 0;
					VoiceMessageState state = VoiceMessageStopped;
					audioVoice()->currentState(&playing, &state);
					if (playing == audio && state != VoiceMessageStopped) {
						audioVoice()->pauseresume();
					} else {
						audioVoice()->play(audio);
					}
				} else {
					QPoint pos(QCursor::pos());
					if (audio->openOnSave < 0 && !psShowOpenWithMenu(pos.x(), pos.y(), already)) {
						psOpenFile(already, true);
					} else {
						psOpenFile(already, audio->openOnSave < 0);
					}
				}
			}
		}
	}
	const AudioItems &items(App::audioItems());
	AudioItems::const_iterator i = items.constFind(audio);
	if (i != items.cend()) {
		for (HistoryItemsMap::const_iterator j = i->cbegin(), e = i->cend(); j != e; ++j) {
			msgUpdated(j.key()->history()->peer->id, j.key());
		}
	}
}

void MainWidget::audioPlayProgress(AudioData *audio) {
	const AudioItems &items(App::audioItems());
	AudioItems::const_iterator i = items.constFind(audio);
	if (i != items.cend()) {
		for (HistoryItemsMap::const_iterator j = i->cbegin(), e = i->cend(); j != e; ++j) {
			msgUpdated(j.key()->history()->peer->id, j.key());
		}
	}
}

void MainWidget::audioLoadFailed(mtpFileLoader *loader, bool started) {
	loadFailed(loader, started, SLOT(audioLoadRetry()));
	AudioData *audio = App::audio(loader->objId());
	if (audio) {
		audio->status = FileFailed;
		if (audio->loader) audio->finish();
	}
}

void MainWidget::audioLoadRetry() {
	App::wnd()->hideLayer();
	AudioData *audio = App::audio(failedObjId);
	if (audio) audio->save(failedFileName);
}

void MainWidget::documentLoadProgress(mtpFileLoader *loader) {
	DocumentData *document = App::document(loader->objId());
	if (document->loader) {
		if (document->loader->done()) {
			document->finish();
			QString already = document->already();
			if (!already.isEmpty() && document->openOnSave) {
				if (document->openOnSave > 0 && document->size < MediaViewImageSizeLimit) {
					QImageReader reader(already);
					if (reader.canRead()) {
						HistoryItem *item = App::histItemById(document->openOnSaveMsgId);
						if (reader.supportsAnimation() && reader.imageCount() > 1 && item) {
							startGif(item, already);
						} else {
							App::wnd()->showDocument(document, item);
						}
					} else {
						psOpenFile(already);
					}
				} else {
					QPoint pos(QCursor::pos());
					if (document->openOnSave < 0 && !psShowOpenWithMenu(pos.x(), pos.y(), already)) {
						psOpenFile(already, true);
					} else {
						psOpenFile(already, document->openOnSave < 0);
					}
				}
			}
		}
	}
	const DocumentItems &items(App::documentItems());
	DocumentItems::const_iterator i = items.constFind(document);
	if (i != items.cend()) {
		for (HistoryItemsMap::const_iterator j = i->cbegin(), e = i->cend(); j != e; ++j) {
			msgUpdated(j.key()->history()->peer->id, j.key());
		}
	}
	App::wnd()->documentUpdated(document);
}

void MainWidget::documentLoadFailed(mtpFileLoader *loader, bool started) {
	loadFailed(loader, started, SLOT(documentLoadRetry()));
	DocumentData *document = App::document(loader->objId());
	if (document) {
		if (document->loader) document->finish();
		document->status = FileFailed;
	}
}

void MainWidget::documentLoadRetry() {
	App::wnd()->hideLayer();
	DocumentData *document = App::document(failedObjId);
	if (document) document->save(failedFileName);
}

void MainWidget::onParentResize(const QSize &newSize) {
	resize(newSize);
}

void MainWidget::updateOnlineDisplay() {
	if (this != App::main()) return;
	history.updateOnlineDisplay(history.x(), width() - history.x() - st::sysBtnDelta * 2 - st::sysCls.img.pxWidth() - st::sysRes.img.pxWidth() - st::sysMin.img.pxWidth());
	if (profile) profile->updateOnlineDisplay();
	if (App::wnd()->settingsWidget()) App::wnd()->settingsWidget()->updateOnlineDisplay();
}

void MainWidget::confirmShareContact(bool ctrlShiftEnter, const QString &phone, const QString &fname, const QString &lname, MsgId replyTo) {
	history.confirmShareContact(ctrlShiftEnter, phone, fname, lname, replyTo);
}

void MainWidget::confirmSendImage(const ReadyLocalMedia &img) {
	history.confirmSendImage(img);
	history.cancelReply();
}

void MainWidget::confirmSendImageUncompressed(bool ctrlShiftEnter, MsgId replyTo) {
	history.uploadConfirmImageUncompressed(ctrlShiftEnter, replyTo);
	history.cancelReply();
}

void MainWidget::cancelSendImage() {
	history.cancelSendImage();
}

void MainWidget::dialogsCancelled() {
	if (hider) {
		hider->startHide();
		noHider(hider);
		history.activate();
	} else {
		history.activate();
	}
}

void MainWidget::serviceNotification(const QString &msg, const MTPMessageMedia &media, bool unread) {
	int32 flags = unread ? MTPDmessage_flag_unread : 0;
	QString sendingText, leftText = msg;
	HistoryItem *item = 0;
	while (textSplit(sendingText, leftText, MaxMessageSize)) {
		item = App::histories().addToBack(MTP_message(MTP_int(flags), MTP_int(clientMsgId()), MTP_int(ServiceUserId), MTP_peerUser(MTP_int(MTP::authedId())), MTPint(), MTPint(), MTPint(), MTP_int(unixtime()), MTP_string(sendingText), media), unread ? 1 : 2);
	}
	if (item) {
		history.peerMessagesUpdated(item->history()->peer->id);
	}
}

void MainWidget::serviceHistoryDone(const MTPmessages_Messages &msgs) {
	switch (msgs.type()) {
	case mtpc_messages_messages:
		App::feedUsers(msgs.c_messages_messages().vusers);
		App::feedChats(msgs.c_messages_messages().vchats);
		App::feedMsgs(msgs.c_messages_messages().vmessages);
		break;

	case mtpc_messages_messagesSlice:
		App::feedUsers(msgs.c_messages_messagesSlice().vusers);
		App::feedChats(msgs.c_messages_messagesSlice().vchats);
		App::feedMsgs(msgs.c_messages_messagesSlice().vmessages);
		break;
	}

	App::wnd()->showDelayedServiceMsgs();
}

bool MainWidget::serviceHistoryFail(const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	App::wnd()->showDelayedServiceMsgs();
	return false;
}

bool MainWidget::isIdle() const {
	return _isIdle;
}

void MainWidget::clearCachedBackground() {
	_cachedBackground = QPixmap();
	_cacheBackgroundTimer.stop();
}

QPixmap MainWidget::cachedBackground(const QRect &forRect, int &x, int &y) {
	if (!_cachedBackground.isNull() && forRect == _cachedFor) {
		x = _cachedX;
		y = _cachedY;
		return _cachedBackground;
	}
	if (_willCacheFor != forRect || !_cacheBackgroundTimer.isActive()) {
		_willCacheFor = forRect;
		_cacheBackgroundTimer.start(CacheBackgroundTimeout);
	}
	return QPixmap();
}

void MainWidget::backgroundParams(const QRect &forRect, QRect &to, QRect &from) const {
	const QSize &bg(cChatBackground()->size());
	if (uint64(bg.width()) * forRect.height() > uint64(bg.height()) * forRect.width()) {
		float64 pxsize = forRect.height() / float64(bg.height());
		int takewidth = qCeil(forRect.width() / pxsize);
		if (takewidth > bg.width()) {
			takewidth = bg.width();
		} else if ((bg.width() % 2) != (takewidth % 2)) {
			++takewidth;
		}
		to = QRect(int((forRect.width() - takewidth * pxsize) / 2.), 0, qCeil(takewidth * pxsize), forRect.height());
		from = QRect((bg.width() - takewidth) / 2, 0, takewidth, bg.height());
	} else {
		float64 pxsize = forRect.width() / float64(bg.width());
		int takeheight = qCeil(forRect.height() / pxsize);
		if (takeheight > bg.height()) {
			takeheight = bg.height();
		} else if ((bg.height() % 2) != (takeheight % 2)) {
			++takeheight;
		}
		to = QRect(0, int((forRect.height() - takeheight * pxsize) / 2.), forRect.width(), qCeil(takeheight * pxsize));
		from = QRect(0, (bg.height() - takeheight) / 2, bg.width(), takeheight);
	}
}

void MainWidget::updateScrollColors() {
	history.updateScrollColors();
	if (overview) overview->updateScrollColors();
}

void MainWidget::setChatBackground(const App::WallPaper &wp) {
	_background = new App::WallPaper(wp);
	_background->full->load();
	checkChatBackground();
}

bool MainWidget::chatBackgroundLoading() {
	return !!_background;
}

void MainWidget::checkChatBackground() {
	if (_background) {
		if (_background->full->loaded()) {
			if (_background->full->isNull()) {
				App::initBackground();
			} else if (_background->id == 0 || _background->id == DefaultChatBackground) {
				App::initBackground(_background->id);
			} else {
				App::initBackground(_background->id, _background->full->pix().toImage());
			}
			delete _background;
			_background = 0;
			QTimer::singleShot(0, this, SLOT(update()));
		}
	}
}

ImagePtr MainWidget::newBackgroundThumb() {
	return _background ? _background->thumb : ImagePtr();
}

ApiWrap *MainWidget::api() {
	return _api;
}

void MainWidget::updateReplyTo() {
	history.updateReplyTo(true);
}

void MainWidget::pushReplyReturn(HistoryItem *item) {
	history.pushReplyReturn(item);
}

void MainWidget::setInnerFocus() {
	if (hider || !history.peer()) {
		if (hider && hider->wasOffered()) {
			hider->setFocus();
		} else if (overview) {
			overview->activate();
		} else if (profile) {
			profile->activate();
		} else {
			dialogs.setInnerFocus();
		}
	} else if (profile) {
		profile->setFocus();
	} else {
		history.activate();
	}
}

void MainWidget::createDialogAtTop(History *history, int32 unreadCount) {
	dialogs.createDialogAtTop(history, unreadCount);
}

void MainWidget::showPeer(quint64 peerId, qint32 msgId, bool back, bool force) {
	if (!back && _stack.size() == 1 && _stack[0]->type() == HistoryStackItem && _stack[0]->peer->id == peerId) {
		if (cWideMode() || !selectingPeer()) {
			back = true;
		}
	}
	App::wnd()->hideLayer();
	QPixmap animCache, animTopBarCache;
	if (force && hider) {
		hider->startHide();
		hider = 0;
	}
	if (force || !selectingPeer()) {
		if (!animating() && ((history.isHidden() && (profile || overview)) || (!cWideMode() && (history.isHidden() || !peerId)))) {
			dialogs.enableShadow(false);
			if (peerId) {
				_topBar.enableShadow(false);
				if (cWideMode()) {
					animCache = myGrab(this, QRect(_dialogsWidth, st::topBarHeight, width() - _dialogsWidth, height() - st::topBarHeight));
				} else {
					animCache = myGrab(this, QRect(0, st::topBarHeight, _dialogsWidth, height() - st::topBarHeight));
				}
			} else if (cWideMode()) {
				animCache = myGrab(this, QRect(_dialogsWidth, 0, width() - _dialogsWidth, height()));
			} else {
				animCache = myGrab(this, QRect(0, 0, _dialogsWidth, height()));
			}
			if (peerId || cWideMode()) {
				animTopBarCache = myGrab(this, QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight));
			}
			dialogs.enableShadow();
			_topBar.enableShadow();
			history.show();
		}
	}
	history.showPeer(peerId, msgId, force);
	if (force || !selectingPeer()) {
		bool noPeer = (!history.peer() || !history.peer()->id), onlyDialogs = noPeer && !cWideMode();
		if (profile || overview) {
			if (profile) {
				profile->hide();
				profile->deleteLater();
				profile->rpcInvalidate();
				profile = 0;
			}
			if (overview) {
				overview->hide();
				overview->clear();
				overview->deleteLater();
				overview->rpcInvalidate();
				overview = 0;
			}
			_stack.clear();
		}
		if (onlyDialogs) {
			_topBar.hide();
			history.hide();
			if (!animating()) {
				dialogs.show();
				if (!animCache.isNull()) {
					dialogs.animShow(animCache);
				}
			}
		} else {
			if (noPeer) {
				_topBar.hide();
				resizeEvent(0);
			}
			if (!cWideMode()) dialogs.hide();
			if (!animating()) {
				history.show();
				if (!animCache.isNull()) {
					history.animShow(animCache, animTopBarCache, back);
				}
			}
		}
	}
	if (!dialogs.isHidden()) {
		dialogs.scrollToPeer(peerId, msgId);
		dialogs.update();
	}
	App::wnd()->getTitle()->updateBackButton();
}

void MainWidget::peerBefore(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) {
	if (selectingPeer()) {
		outPeer = 0;
		outMsg = 0;
		return;
	}
	dialogs.peerBefore(inPeer, inMsg, outPeer, outMsg);
}

void MainWidget::peerAfter(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) {
	if (selectingPeer()) {
		outPeer = 0;
		outMsg = 0;
		return;
	}
	dialogs.peerAfter(inPeer, inMsg, outPeer, outMsg);
}

PeerData *MainWidget::historyPeer() {
	return history.peer();
}

PeerData *MainWidget::peer() {
	return overview ? overview->peer() : history.peer();
}

PeerData *MainWidget::activePeer() {
	return history.activePeer();
}

MsgId MainWidget::activeMsgId() {
	return history.activeMsgId();
}

PeerData *MainWidget::profilePeer() {
	return profile ? profile->peer() : 0;
}

bool MainWidget::mediaTypeSwitch() {
	if (!overview) return false;

	for (int32 i = 0; i < OverviewCount; ++i) {
		if (!(_mediaTypeMask & ~(1 << i))) {
			return false;
		}
	}
	return true;
}

void MainWidget::showMediaOverview(PeerData *peer, MediaOverviewType type, bool back, int32 lastScrollTop) {
	App::wnd()->hideSettings();
	if (overview && overview->peer() == peer) {
		if (overview->type() != type) {
			overview->switchType(type);
		}
		return;
	}

	dialogs.enableShadow(false);
	_topBar.enableShadow(false);
	QPixmap animCache = myGrab(this, history.geometry()), animTopBarCache = myGrab(this, QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight));
	dialogs.enableShadow();
	_topBar.enableShadow();
	if (!back) {
		if (overview) {
			_stack.push_back(new StackItemOverview(overview->peer(), overview->type(), overview->lastWidth(), overview->lastScrollTop()));
		} else if (profile) {
			_stack.push_back(new StackItemProfile(profile->peer(), profile->lastScrollTop(), profile->allMediaShown()));
		} else {
			_stack.push_back(new StackItemHistory(history.peer(), history.lastWidth(), history.lastScrollTop(), history.replyReturns()));
		}
	}
	if (overview) {
		overview->hide();
		overview->clear();
		overview->deleteLater();
		overview->rpcInvalidate();
	}
	if (profile) {
		profile->hide();
		profile->deleteLater();
		profile->rpcInvalidate();
		profile = 0;
	}
	overview = new OverviewWidget(this, peer, type);
	_mediaTypeMask = 0;
	mediaOverviewUpdated(peer);
	_topBar.show();
	resizeEvent(0);
	overview->animShow(animCache, animTopBarCache, back, lastScrollTop);
	history.animStop();
	history.showPeer(0, 0, false, true);
	history.hide();
	_topBar.raise();
	dialogs.raise();
	_mediaType.raise();
	if (hider) hider->raise();
	App::wnd()->getTitle()->updateBackButton();
}

void MainWidget::showPeerProfile(PeerData *peer, bool back, int32 lastScrollTop, bool allMediaShown) {
	App::wnd()->hideSettings();
	if (profile && profile->peer() == peer) return;

	dialogs.enableShadow(false);
	_topBar.enableShadow(false);
	QPixmap animCache = myGrab(this, history.geometry()), animTopBarCache = myGrab(this, QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight));
	dialogs.enableShadow();
	_topBar.enableShadow();
	if (!back) {
		if (overview) {
			_stack.push_back(new StackItemOverview(overview->peer(), overview->type(), overview->lastWidth(), overview->lastScrollTop()));
		} else if (profile) {
			_stack.push_back(new StackItemProfile(profile->peer(), profile->lastScrollTop(), profile->allMediaShown()));
		} else {
			_stack.push_back(new StackItemHistory(history.peer(), history.lastWidth(), history.lastScrollTop(), history.replyReturns()));
		}
	}
	if (overview) {
		overview->hide();
		overview->clear();
		overview->deleteLater();
		overview->rpcInvalidate();
		overview = 0;
	}
	if (profile) {
		profile->hide();
		profile->deleteLater();
		profile->rpcInvalidate();
	}
	profile = new ProfileWidget(this, peer);
	_topBar.show();
	resizeEvent(0);
	profile->animShow(animCache, animTopBarCache, back, lastScrollTop, allMediaShown);
	history.animStop();
	history.showPeer(0, 0, false, true);
	history.hide();
	_topBar.raise();
	dialogs.raise();
	_mediaType.raise();
	if (hider) hider->raise();
	App::wnd()->getTitle()->updateBackButton();
}

void MainWidget::showBackFromStack() {
	if (_stack.isEmpty() || selectingPeer()) return;
	StackItem *item = _stack.back();
	_stack.pop_back();
	if (item->type() == HistoryStackItem) {
		StackItemHistory *histItem = static_cast<StackItemHistory*>(item);
		showPeer(histItem->peer->id, App::main()->activeMsgId(), true);
		history.setReplyReturns(histItem->peer->id, histItem->replyReturns);
	} else if (item->type() == ProfileStackItem) {
		StackItemProfile *profItem = static_cast<StackItemProfile*>(item);
		showPeerProfile(profItem->peer, true, profItem->lastScrollTop, profItem->allMediaShown);
	} else if (item->type() == OverviewStackItem) {
		StackItemOverview *overItem = static_cast<StackItemOverview*>(item);
		showMediaOverview(overItem->peer, overItem->mediaType, true, overItem->lastScrollTop);
	}
	delete item;
}

QRect MainWidget::historyRect() const {
	QRect r(history.historyRect());
	r.moveLeft(r.left() + history.x());
	r.moveTop(r.top() + history.y());
	return r;
}

void MainWidget::dlgUpdated(DialogRow *row) {
	dialogs.dlgUpdated(row);
}

void MainWidget::dlgUpdated(History *row) {
	dialogs.dlgUpdated(row);
}

void MainWidget::windowShown() {
	history.windowShown();
}

void MainWidget::sentDataReceived(uint64 randomId, const MTPmessages_SentMessage &result) {
	switch (result.type()) {
	case mtpc_messages_sentMessage: {
		const MTPDmessages_sentMessage &d(result.c_messages_sentMessage());

		if (randomId) feedUpdate(MTP_updateMessageID(d.vid, MTP_long(randomId))); // ignore real date

		if (updInited) {
			if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
				_byPtsSentMessage.insert(ptsKey(SkippedSentMessage), result);
				return;
			}
		}

		HistoryItem *item = App::histItemById(d.vid.v);
		if (item) {
			item->setMedia(d.vmedia);
		}
	} break;

	case mtpc_messages_sentMessageLink: {
		const MTPDmessages_sentMessageLink &d(result.c_messages_sentMessageLink());

		if (randomId) feedUpdate(MTP_updateMessageID(d.vid, MTP_long(randomId))); // ignore real date

		if (updInited) {
			if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
				_byPtsSentMessage.insert(ptsKey(SkippedSentMessage), result);
				return;
			}
		}
		App::feedUserLinks(d.vlinks);

		if (d.vmedia.type() != mtpc_messageMediaEmpty) {
			HistoryItem *item = App::histItemById(d.vid.v);
			if (item) {
				item->setMedia(d.vmedia);
			}
		}
	} break;
	};
}

void MainWidget::sentUpdatesReceived(const MTPUpdates &result) {
	handleUpdates(result);
}

void MainWidget::msgUpdated(PeerId peer, const HistoryItem *msg) {
	if (!msg) return;
	history.msgUpdated(peer, msg);
	if (!msg->history()->dialogs.isEmpty()) dialogs.dlgUpdated(msg->history()->dialogs[0]);
	if (overview) overview->msgUpdated(peer, msg);
}

void MainWidget::historyToDown(History *hist) {
	history.historyToDown(hist);
}

void MainWidget::dialogsToUp() {
	dialogs.dialogsToUp();
}

void MainWidget::newUnreadMsg(History *hist, HistoryItem *item) {
	history.newUnreadMsg(hist, item);
}

void MainWidget::historyWasRead() {
	history.historyWasRead(false);
}

void MainWidget::animShow(const QPixmap &bgAnimCache, bool back) {
	_bgAnimCache = bgAnimCache;

	anim::stop(this);
	showAll();
	_animCache = myGrab(this, rect());

	a_coord = back ? anim::ivalue(-st::introSlideShift, 0) : anim::ivalue(st::introSlideShift, 0);
	a_alpha = anim::fvalue(0, 1);
	a_bgCoord = back ? anim::ivalue(0, st::introSlideShift) : anim::ivalue(0, -st::introSlideShift);
	a_bgAlpha = anim::fvalue(1, 0);

	hideAll();
	anim::start(this);
	show();
}

bool MainWidget::animStep(float64 ms) {
	float64 fullDuration = st::introSlideDelta + st::introSlideDuration, dt = ms / fullDuration;
	float64 dt1 = (ms > st::introSlideDuration) ? 1 : (ms / st::introSlideDuration), dt2 = (ms > st::introSlideDelta) ? (ms - st::introSlideDelta) / (st::introSlideDuration) : 0;
	bool res = true;
	if (dt2 >= 1) {
		res = false;
		a_bgCoord.finish();
		a_bgAlpha.finish();
		a_coord.finish();
		a_alpha.finish();

		_animCache = _bgAnimCache = QPixmap();

		anim::stop(this);
		showAll();
		activate();
	} else {
		a_bgCoord.update(dt1, st::introHideFunc);
		a_bgAlpha.update(dt1, st::introAlphaHideFunc);
		a_coord.update(dt2, st::introShowFunc);
		a_alpha.update(dt2, st::introAlphaShowFunc);
	}
	update();
	return res;
}

void MainWidget::paintEvent(QPaintEvent *e) {
    if (_background) checkChatBackground();

    QPainter p(this);
    if (animating()) {
        p.setOpacity(a_bgAlpha.current());
        p.drawPixmap(a_bgCoord.current(), 0, _bgAnimCache);
        p.setOpacity(a_alpha.current());
        p.drawPixmap(a_coord.current(), 0, _animCache);
    } else {
    }
}

void MainWidget::hideAll() {
	dialogs.hide();
	history.hide();
	if (profile) {
		profile->hide();
	}
	if (overview) {
		overview->hide();
	}
	_topBar.hide();
	_mediaType.hide();
}

void MainWidget::showAll() {
	if (cPasswordRecovered()) {
		cSetPasswordRecovered(false);
		App::wnd()->showLayer(new ConfirmBox(lang(lng_signin_password_removed), true, lang(lng_continue)));
	}
	if (cWideMode()) {
		if (hider) {
			hider->show();
			if (_forwardConfirm) {
				App::wnd()->hideLayer(true);
				_forwardConfirm = 0;
			}
		}
		dialogs.show();
		if (overview) {
			overview->show();
		} else if (profile) {
			profile->show();
		} else {
			history.show();
			history.resizeEvent(0);
		}
		if (profile || overview || history.peer()) {
			_topBar.show();
		}
	} else {
		if (hider) {
			hider->hide();
			if (!_forwardConfirm && hider->wasOffered()) {
				_forwardConfirm = new ConfirmBox(hider->offeredText(), lang(lng_forward));
				connect(_forwardConfirm, SIGNAL(confirmed()), hider, SLOT(forward()));
				connect(_forwardConfirm, SIGNAL(cancelled()), this, SLOT(onForwardCancel()));
				App::wnd()->showLayer(_forwardConfirm, true);
			}
		}
		if (selectingPeer()) {
			dialogs.show();
			history.hide();
			if (overview) overview->hide();
			if (profile) profile->hide();
			_topBar.hide();
		} else if (overview) {
			overview->show();
		} else if (profile) {
			profile->show();
		} else if (history.peer()) {
			history.show();
			history.resizeEvent(0);
		} else {
			dialogs.show();
			history.hide();
		}
		if (!selectingPeer() && (profile || overview || history.peer())) {
			_topBar.show();
			dialogs.hide();
		}
	}
	App::wnd()->checkHistoryActivation();
}

void MainWidget::resizeEvent(QResizeEvent *e) {
	int32 tbh = _topBar.isHidden() ? 0 : st::topBarHeight;
	if (cWideMode()) {
		_dialogsWidth = snap<int>((width() * 5) / 14, st::dlgMinWidth, st::dlgMaxWidth);
		dialogs.setGeometry(0, 0, _dialogsWidth + st::dlgShadow, height());
		_topBar.setGeometry(_dialogsWidth, 0, width() - _dialogsWidth, st::topBarHeight + st::titleShadow);
		history.setGeometry(_dialogsWidth, tbh, width() - _dialogsWidth, height() - tbh);
		if (hider) hider->setGeometry(QRect(_dialogsWidth, 0, width() - _dialogsWidth, height()));
	} else {
		_dialogsWidth = width();
		dialogs.setGeometry(0, 0, _dialogsWidth + st::dlgShadow, height());
		_topBar.setGeometry(0, 0, _dialogsWidth, st::topBarHeight + st::titleShadow);
		history.setGeometry(0, tbh, _dialogsWidth, height() - tbh);
		if (hider) hider->setGeometry(QRect(0, 0, _dialogsWidth, height()));
	}
	_mediaType.move(width() - _mediaType.width(), st::topBarHeight);
	if (profile) profile->setGeometry(history.geometry());
	if (overview) overview->setGeometry(history.geometry());
}

void MainWidget::keyPressEvent(QKeyEvent *e) {
}

void MainWidget::updateWideMode() {
	showAll();
	_topBar.showAll();
}

bool MainWidget::needBackButton() {
	return overview || profile || (history.peer() && history.peer()->id);
}

void MainWidget::onShowDialogs() {
	showPeer(0, 0, false, true);
}

void MainWidget::paintTopBar(QPainter &p, float64 over, int32 decreaseWidth) {
	if (profile) {
		profile->paintTopBar(p, over, decreaseWidth);
	} else if (overview) {
		overview->paintTopBar(p, over, decreaseWidth);
	} else {
		history.paintTopBar(p, over, decreaseWidth);
	}
}

void MainWidget::topBarShadowParams(int32 &x, float64 &o) {
	if (!profile && !overview && dialogs.isHidden()) {
		history.topBarShadowParams(x, o);
	}
}

void MainWidget::onPhotosSelect() {
	if (overview) overview->switchType(OverviewPhotos);
	_mediaType.hideStart();
}

void MainWidget::onVideosSelect() {
	if (overview) overview->switchType(OverviewVideos);
	_mediaType.hideStart();
}

void MainWidget::onDocumentsSelect() {
	if (overview) overview->switchType(OverviewDocuments);
	_mediaType.hideStart();
}

void MainWidget::onAudiosSelect() {
	if (overview) overview->switchType(OverviewAudios);
	_mediaType.hideStart();
}

TopBarWidget *MainWidget::topBar() {
	return &_topBar;
}

void MainWidget::onTopBarClick() {
    if (profile) {
		profile->topBarClick();
	} else if (overview) {
		overview->topBarClick();
	} else {
		history.topBarClick();
	}
}

void MainWidget::onPeerShown(PeerData *peer) {
	if ((cWideMode() || !selectingPeer()) && (profile || overview || (peer && peer->id))) {
		_topBar.show();
	} else {
		_topBar.hide();
	}
	resizeEvent(0);
	if (animating()) _topBar.hide();
}

void MainWidget::onUpdateNotifySettings() {
	if (this != App::main()) return;
	while (!updateNotifySettingPeers.isEmpty()) {
		PeerData *peer = *updateNotifySettingPeers.begin();
		updateNotifySettingPeers.erase(updateNotifySettingPeers.begin());

		if (peer->notify == UnknownNotifySettings || peer->notify == EmptyNotifySettings) {
			peer->notify = new NotifySettings();
		}
		MTP::send(MTPaccount_UpdateNotifySettings(MTP_inputNotifyPeer(peer->input), MTP_inputPeerNotifySettings(MTP_int(peer->notify->mute), MTP_string(peer->notify->sound), MTP_bool(peer->notify->previews), MTP_int(peer->notify->events))), RPCResponseHandler(), 0, updateNotifySettingPeers.isEmpty() ? 0 : 10);
	}
}

void MainWidget::feedUpdates(const MTPVector<MTPUpdate> &updates, bool skipMessageIds) {
	const QVector<MTPUpdate> &v(updates.c_vector().v);
	for (QVector<MTPUpdate>::const_iterator i = v.cbegin(), e = v.cend(); i != e; ++i) {
		if (skipMessageIds && i->type() == mtpc_updateMessageID) continue;
		feedUpdate(*i);
	}
}

void MainWidget::feedMessageIds(const MTPVector<MTPUpdate> &updates) {
	const QVector<MTPUpdate> &v(updates.c_vector().v);
	for (QVector<MTPUpdate>::const_iterator i = v.cbegin(), e = v.cend(); i != e; ++i) {
		if (i->type() == mtpc_updateMessageID) {
			feedUpdate(*i);
		}
	}
}

bool MainWidget::updateFail(const RPCError &e) {
	if (MTP::authedId()) {
		App::logOut();
	}
	return true;
}

void MainWidget::updSetState(int32 pts, int32 date, int32 qts, int32 seq) {
	if (pts) updGoodPts = updLastPts = updPtsCount = pts;
	if (updDate < date) updDate = date;
	if (qts && updQts < qts) {
		updQts = qts;
	}
	if (seq && seq != updSeq) {
		updSeq = seq;
		if (_bySeqTimer.isActive()) _bySeqTimer.stop();
		for (QMap<int32, MTPUpdates>::iterator i = _bySeqUpdates.begin(); i != _bySeqUpdates.end();) {
			int32 s = i.key();
			if (s <= seq + 1) {
				MTPUpdates v = i.value();
				i = _bySeqUpdates.erase(i);
				if (s == seq + 1) {
					return handleUpdates(v);
				}
			} else {
				if (!_bySeqTimer.isActive()) _bySeqTimer.start(WaitForSkippedTimeout);
				break;
			}
		}
	}
}

void MainWidget::gotState(const MTPupdates_State &state) {
	const MTPDupdates_state &d(state.c_updates_state());
	updSetState(d.vpts.v, d.vdate.v, d.vqts.v, d.vseq.v);

	MTP::setGlobalDoneHandler(rpcDone(&MainWidget::updateReceived));
	_lastUpdateTime = getms(true);
	noUpdatesTimer.start(NoUpdatesTimeout);
	updInited = true;

	dialogs.loadDialogs();
	updateOnline();
}

void MainWidget::gotDifference(const MTPupdates_Difference &diff) {
	_failDifferenceTimeout = 1;

	switch (diff.type()) {
	case mtpc_updates_differenceEmpty: {
		const MTPDupdates_differenceEmpty &d(diff.c_updates_differenceEmpty());
		updSetState(updGoodPts, d.vdate.v, updQts, d.vseq.v);

		MTP::setGlobalDoneHandler(rpcDone(&MainWidget::updateReceived));
		_lastUpdateTime = getms(true);
		noUpdatesTimer.start(NoUpdatesTimeout);

		updInited = true;
	} break;
	case mtpc_updates_differenceSlice: {
		const MTPDupdates_differenceSlice &d(diff.c_updates_differenceSlice());
		feedDifference(d.vusers, d.vchats, d.vnew_messages, d.vother_updates);

		const MTPDupdates_state &s(d.vintermediate_state.c_updates_state());
		updSetState(s.vpts.v, s.vdate.v, s.vqts.v, s.vseq.v);

		updInited = true;

		MTP_LOG(0, ("getDifference { good - after a slice of difference was received }"));
		getDifference();
	} break;
	case mtpc_updates_difference: {
		const MTPDupdates_difference &d(diff.c_updates_difference());
		feedDifference(d.vusers, d.vchats, d.vnew_messages, d.vother_updates);

		gotState(d.vstate);
	} break;
	};
}

uint64 MainWidget::ptsKey(PtsSkippedQueue queue) {
	return _byPtsQueue.insert(uint64(uint32(updLastPts)) << 32 | uint64(uint32(updPtsCount)), queue).key();
}

void MainWidget::applySkippedPtsUpdates() {
	if (_byPtsTimer.isActive()) _byPtsTimer.stop();
	if (_byPtsQueue.isEmpty()) return;
	++updSkipPtsUpdateLevel;
	for (QMap<uint64, PtsSkippedQueue>::const_iterator i = _byPtsQueue.cbegin(), e = _byPtsQueue.cend(); i != e; ++i) {
		switch (i.value()) {
		case SkippedUpdate: feedUpdate(_byPtsUpdate.value(i.key())); break;
		case SkippedUpdates: handleUpdates(_byPtsUpdates.value(i.key())); break;
		case SkippedSentMessage: sentDataReceived(0, _byPtsSentMessage.value(i.key())); break;
		}
	}
	--updSkipPtsUpdateLevel;
	clearSkippedPtsUpdates();
}

void MainWidget::clearSkippedPtsUpdates() {
	_byPtsQueue.clear();
	_byPtsUpdate.clear();
	_byPtsUpdates.clear();
	_byPtsSentMessage.clear();
	updSkipPtsUpdateLevel = 0;
}

bool MainWidget::updPtsUpdated(int pts, int ptsCount) { // return false if need to save that update and apply later
	if (!updInited || updSkipPtsUpdateLevel) return true;

	updLastPts = qMax(updLastPts, pts);
	updPtsCount += ptsCount;
	if (updLastPts == updPtsCount) {
		applySkippedPtsUpdates();
		updGoodPts = updLastPts;
		return true;
	} else if (updLastPts < updPtsCount) {
		_byPtsTimer.startIfNotActive(1);
	} else {
		_byPtsTimer.startIfNotActive(WaitForSkippedTimeout);
	}
	return !ptsCount;
}

void MainWidget::feedDifference(const MTPVector<MTPUser> &users, const MTPVector<MTPChat> &chats, const MTPVector<MTPMessage> &msgs, const MTPVector<MTPUpdate> &other) {
	App::wnd()->checkAutoLock();
	App::feedUsers(users);
	App::feedChats(chats);
	feedMessageIds(other);
	App::feedMsgs(msgs, 1);
	feedUpdates(other, true);
	history.peerMessagesUpdated();
}

bool MainWidget::failDifference(const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	LOG(("RPC Error: %1 %2: %3").arg(error.code()).arg(error.type()).arg(error.description()));
	_failDifferenceTimer.start(_failDifferenceTimeout * 1000);
	if (_failDifferenceTimeout < 64) _failDifferenceTimeout *= 2;
	return true;
}

void MainWidget::getDifferenceForce() {
	if (MTP::authedId()) {
		updInited = true;
		MTP_LOG(0, ("getDifference { force - after get difference failed }"));
		getDifference();
	}
}

void MainWidget::getDifference() {
	if (this != App::main()) return;

	LOG(("Getting difference! no updates timer: %1, remains: %2").arg(noUpdatesTimer.isActive() ? 1 : 0).arg(noUpdatesTimer.remainingTime()));
	if (!updInited) return;

	_bySeqUpdates.clear();
	_bySeqTimer.stop();

	clearSkippedPtsUpdates();
	_byPtsTimer.stop();

	noUpdatesTimer.stop();
	_failDifferenceTimer.stop();

	LOG(("Getting difference for %1, %2").arg(updGoodPts).arg(updDate));
	updInited = false;
	MTP::setGlobalDoneHandler(RPCDoneHandlerPtr(0));
	MTP::send(MTPupdates_GetDifference(MTP_int(updGoodPts), MTP_int(updDate), MTP_int(updQts)), rpcDone(&MainWidget::gotDifference), rpcFail(&MainWidget::failDifference));
}

void MainWidget::mtpPing() {
	MTP::ping();
}

void MainWidget::start(const MTPUser &user) {
	int32 uid = user.c_userSelf().vid.v;
	if (MTP::authedId() != uid) {
		MTP::authed(uid);
		Local::writeMtpData();
	}

	cSetOtherOnline(0);
	App::feedUsers(MTP_vector<MTPUser>(1, user));
	App::app()->startUpdateCheck();
	MTP::send(MTPupdates_GetState(), rpcDone(&MainWidget::gotState));
	update();
	if (!cStartUrl().isEmpty()) {
		openLocalUrl(cStartUrl());
		cSetStartUrl(QString());
	}
	_started = true;
	App::wnd()->sendServiceHistoryRequest();
	Local::readRecentStickers();
	history.start();
}

bool MainWidget::started() {
	return _started;
}

void MainWidget::openLocalUrl(const QString &url) {
	QString u(url.trimmed());
	if (u.startsWith(QLatin1String("tg://resolve"), Qt::CaseInsensitive)) {
		QRegularExpressionMatch m = QRegularExpression(qsl("^tg://resolve/?\\?domain=([a-zA-Z0-9\\.\\_]+)$"), QRegularExpression::CaseInsensitiveOption).match(u);
		if (m.hasMatch()) {
			openUserByName(m.captured(1));
		}
	} else if (u.startsWith(QLatin1String("tg://join"), Qt::CaseInsensitive)) {
		QRegularExpressionMatch m = QRegularExpression(qsl("^tg://join/?\\?invite=([a-zA-Z0-9\\.\\_]+)$"), QRegularExpression::CaseInsensitiveOption).match(u);
		if (m.hasMatch()) {
		}
	}
}

void MainWidget::openUserByName(const QString &username, bool toProfile) {
	UserData *user = App::userByName(username);
	if (user) {
		if (toProfile) {
			showPeerProfile(user);
		} else {
			emit showPeerAsync(user->id, 0, false, true);
		}
	} else {
		MTP::send(MTPcontacts_ResolveUsername(MTP_string(username)), rpcDone(&MainWidget::usernameResolveDone, toProfile), rpcFail(&MainWidget::usernameResolveFail, username));
	}
}

void MainWidget::usernameResolveDone(bool toProfile, const MTPUser &user) {
	App::wnd()->hideLayer();
	UserData *u = App::feedUsers(MTP_vector<MTPUser>(1, user));
	if (toProfile) {
		showPeerProfile(u);
	} else {
		showPeer(u->id, 0, false, true);
	}
}

bool MainWidget::usernameResolveFail(QString name, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	if (error.code() == 400) {
		App::wnd()->showLayer(new ConfirmBox(lng_username_not_found(lt_user, name), true));
	}
	return true;
}

void MainWidget::startFull(const MTPVector<MTPUser> &users) {
	const QVector<MTPUser> &v(users.c_vector().v);
	if (v.isEmpty() || v[0].type() != mtpc_userSelf) { // wtf?..
		return App::logOut();
	}
	start(v[0]);
}

void MainWidget::applyNotifySetting(const MTPNotifyPeer &peer, const MTPPeerNotifySettings &settings, History *history) {
	switch (settings.type()) {
	case mtpc_peerNotifySettingsEmpty:
		switch (peer.type()) {
		case mtpc_notifyAll: globalNotifyAllPtr = EmptyNotifySettings; break;
		case mtpc_notifyUsers: globalNotifyUsersPtr = EmptyNotifySettings; break;
		case mtpc_notifyChats: globalNotifyChatsPtr = EmptyNotifySettings; break;
		case mtpc_notifyPeer: {
			PeerData *data = App::peerLoaded(App::peerFromMTP(peer.c_notifyPeer().vpeer));
			if (data && data->notify != EmptyNotifySettings) {
				if (data->notify != UnknownNotifySettings) {
					delete data->notify;
				}
				data->notify = EmptyNotifySettings;
				App::history(data->id)->setMute(false);
			}
		} break;
		}
	break;
	case mtpc_peerNotifySettings: {
		const MTPDpeerNotifySettings &d(settings.c_peerNotifySettings());
		NotifySettingsPtr setTo = UnknownNotifySettings;
		PeerId peerId = 0;
		switch (peer.type()) {
		case mtpc_notifyAll: setTo = globalNotifyAllPtr = &globalNotifyAll; break;
		case mtpc_notifyUsers: setTo = globalNotifyUsersPtr = &globalNotifyUsers; break;
		case mtpc_notifyChats: setTo = globalNotifyChatsPtr = &globalNotifyChats; break;
		case mtpc_notifyPeer: {
			PeerData *data = App::peerLoaded(App::peerFromMTP(peer.c_notifyPeer().vpeer));
			if (!data) break;

			peerId = data->id;
			if (data->notify == UnknownNotifySettings || data->notify == EmptyNotifySettings) {
				data->notify = new NotifySettings();
			}
			setTo = data->notify;
		} break;
		}
		if (setTo == UnknownNotifySettings) break;

		setTo->mute = d.vmute_until.v;
		setTo->sound = d.vsound.c_string().v;
		setTo->previews = d.vshow_previews.v;
		setTo->events = d.vevents_mask.v;
		if (peerId) {
			if (!history) history = App::history(peerId);
			if (isNotifyMuted(setTo)) {
				App::wnd()->notifyClear(history);
				history->setMute(true);
			} else {
				history->setMute(false);
			}
		}
	} break;
	}

	if (profile) {
		profile->updateNotifySettings();
	}
}

void MainWidget::gotNotifySetting(MTPInputNotifyPeer peer, const MTPPeerNotifySettings &settings) {
	switch (peer.type()) {
	case mtpc_inputNotifyAll: applyNotifySetting(MTP_notifyAll(), settings); break;
	case mtpc_inputNotifyUsers: applyNotifySetting(MTP_notifyUsers(), settings); break;
	case mtpc_inputNotifyChats: applyNotifySetting(MTP_notifyChats(), settings); break;
	case mtpc_inputNotifyGeoChatPeer: break; // no MTP_peerGeoChat
	case mtpc_inputNotifyPeer:
		switch (peer.c_inputNotifyPeer().vpeer.type()) {
		case mtpc_inputPeerEmpty: applyNotifySetting(MTP_notifyPeer(MTP_peerUser(MTP_int(0))), settings); break;
		case mtpc_inputPeerSelf: applyNotifySetting(MTP_notifyPeer(MTP_peerUser(MTP_int(MTP::authedId()))), settings); break;
		case mtpc_inputPeerContact: applyNotifySetting(MTP_notifyPeer(MTP_peerUser(peer.c_inputNotifyPeer().vpeer.c_inputPeerContact().vuser_id)), settings); break;
		case mtpc_inputPeerForeign: applyNotifySetting(MTP_notifyPeer(MTP_peerUser(peer.c_inputNotifyPeer().vpeer.c_inputPeerForeign().vuser_id)), settings); break;
		case mtpc_inputPeerChat: applyNotifySetting(MTP_notifyPeer(MTP_peerChat(peer.c_inputNotifyPeer().vpeer.c_inputPeerChat().vchat_id)), settings); break;
		}
	break;
	}
	App::wnd()->notifySettingGot();
}

bool MainWidget::failNotifySetting(MTPInputNotifyPeer peer, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	gotNotifySetting(peer, MTP_peerNotifySettingsEmpty());
	return true;
}

void MainWidget::updateNotifySetting(PeerData *peer, bool enabled) {
	updateNotifySettingPeers.insert(peer);
	if (peer->notify == EmptyNotifySettings) {
		if (!enabled) {
			peer->notify = new NotifySettings();
			peer->notify->sound = "";
			peer->notify->mute = unixtime() + 86400 * 365;
		}
	} else {
		if (peer->notify == UnknownNotifySettings) {
			peer->notify = new NotifySettings();
		}
		peer->notify->sound = enabled ? "default" : "";
		peer->notify->mute = enabled ? 0 : (unixtime() + 86400 * 365);
	}
	App::history(peer->id)->setMute(!enabled);
	updateNotifySettingTimer.start(NotifySettingSaveTimeout);
}

void MainWidget::incrementSticker(DocumentData *sticker) {
	RecentStickerPack recent(cRecentStickers());
	RecentStickerPack::iterator i = recent.begin(), e = recent.end();
	for (; i != e; ++i) {
		if (i->first == sticker) {
			if (i->second > 0) {
				++i->second;
			} else {
				--i->second;
			}
			if (qAbs(i->second) > 0x4000) {
				for (RecentStickerPack::iterator j = recent.begin(); j != e; ++j) {
					if (qAbs(j->second) > 1) {
						j->second /= 2;
					} else if (j->second > 0) {
						j->second = 1;
					} else {
						j->second = -1;
					}
				}
			}
			for (; i != recent.begin(); --i) {
				if (qAbs((i - 1)->second) > qAbs(i->second)) {
					break;
				}
				qSwap(*i, *(i - 1));
			}
			break;
		}
	}
	if (i == e) {
		recent.push_front(qMakePair(sticker, -(recent.isEmpty() ? 1 : qAbs(recent.front().second))));
	}
	cSetRecentStickers(recent);
	Local::writeRecentStickers();

	history.updateRecentStickers();
}

void MainWidget::activate() {
	if (!profile && !overview) {
		if (hider) {
			if (hider->wasOffered()) {
				hider->setFocus();
			} else {
				dialogs.activate();
			}
        } else if (App::wnd() && !App::wnd()->layerShown()) {
			if (!cSendPaths().isEmpty()) {
				forwardLayer(-1);
			} else if (history.peer()) {
				history.activate();
			} else {
				dialogs.activate();
			}
		}
	}
	App::wnd()->fixOrder();
}

void MainWidget::destroyData() {
	history.destroyData();
	dialogs.destroyData();
}

void MainWidget::updateOnlineDisplayIn(int32 msecs) {
	_onlineUpdater.start(msecs);
}

void MainWidget::addNewContact(int32 uid, bool show) {
	if (dialogs.addNewContact(uid, show)) {
		showPeer(App::peerFromUser(uid));
	}
}

bool MainWidget::isActive() const {
	return !_isIdle && isVisible() && !animating();
}

bool MainWidget::historyIsActive() const {
	return isActive() && !profile && !overview && history.isActive();
}

bool MainWidget::lastWasOnline() const {
	return _lastWasOnline;
}

uint64 MainWidget::lastSetOnline() const {
	return _lastSetOnline;
}

int32 MainWidget::dlgsWidth() const {
	return dialogs.width();
}

MainWidget::~MainWidget() {
	if (App::main() == this) history.showPeer(0, 0, true);

	delete _background;

	delete hider;
	MTP::clearGlobalHandlers();
	delete _api;
	if (App::wnd()) App::wnd()->noMain(this);
}

void MainWidget::updateOnline(bool gotOtherOffline) {
	if (this != App::main()) return;
	App::wnd()->checkAutoLock();

	bool isOnline = App::wnd()->isActive();
	int updateIn = cOnlineUpdatePeriod();
	if (isOnline) {
		uint64 idle = psIdleTime();
		if (idle >= uint64(cOfflineIdleTimeout())) {
			isOnline = false;
			if (!_isIdle) {
				_isIdle = true;
				_idleFinishTimer.start(900);
			}
		} else {
			updateIn = qMin(updateIn, int(cOfflineIdleTimeout() - idle));
		}
	}
	uint64 ms = getms(true);
	if (isOnline != _lastWasOnline || (isOnline && _lastSetOnline + cOnlineUpdatePeriod() <= ms) || (isOnline && gotOtherOffline)) {
		if (_onlineRequest) {
			MTP::cancel(_onlineRequest);
			_onlineRequest = 0;
		}

		_lastWasOnline = isOnline;
		_lastSetOnline = ms;
		_onlineRequest = MTP::send(MTPaccount_UpdateStatus(MTP_bool(!isOnline)));

		if (App::self()) App::self()->onlineTill = unixtime() + (isOnline ? (cOnlineUpdatePeriod() / 1000) : -1);

		_lastSetOnline = getms(true);

		updateOnlineDisplay();
	} else if (isOnline) {
		updateIn = qMin(updateIn, int(_lastSetOnline + cOnlineUpdatePeriod() - ms));
	}
	_onlineTimer.start(updateIn);
}

void MainWidget::checkIdleFinish() {
	if (this != App::main()) return;
	if (psIdleTime() < uint64(cOfflineIdleTimeout())) {
		_idleFinishTimer.stop();
		_isIdle = false;
		updateOnline();
		if (App::wnd()) App::wnd()->checkHistoryActivation();
	} else {
		_idleFinishTimer.start(900);
	}
}

void MainWidget::updateReceived(const mtpPrime *from, const mtpPrime *end) {
	if (end <= from || !MTP::authedId()) return;

	App::wnd()->checkAutoLock();

	if (mtpTypeId(*from) == mtpc_new_session_created) {
		MTPNewSession newSession(from, end);
		updSeq = 0;
		MTP_LOG(0, ("getDifference { after new_session_created }"));
		return getDifference();
	} else {
		try {
			MTPUpdates updates(from, end);

			_lastUpdateTime = getms(true);
			noUpdatesTimer.start(NoUpdatesTimeout);

			handleUpdates(updates);
		} catch(mtpErrorUnexpected &e) { // just some other type
		}
	}
	update();
}

void MainWidget::handleUpdates(const MTPUpdates &updates) {
	switch (updates.type()) {
	case mtpc_updates: {
		const MTPDupdates &d(updates.c_updates());
		if (d.vseq.v) {
			if (d.vseq.v <= updSeq) return;
			if (d.vseq.v > updSeq + 1) {
				_bySeqUpdates.insert(d.vseq.v, updates);
				return _bySeqTimer.start(WaitForSkippedTimeout);
			}
		}

		App::feedChats(d.vchats);
		App::feedUsers(d.vusers);
		feedUpdates(d.vupdates);

		updSetState(0, d.vdate.v, updQts, d.vseq.v);
	} break;

	case mtpc_updatesCombined: {
		const MTPDupdatesCombined &d(updates.c_updatesCombined());
		if (d.vseq_start.v) {
			if (d.vseq_start.v <= updSeq) return;
			if (d.vseq_start.v > updSeq + 1) {
				_bySeqUpdates.insert(d.vseq_start.v, updates);
				return _bySeqTimer.start(WaitForSkippedTimeout);
			}
		}

		App::feedChats(d.vchats);
		App::feedUsers(d.vusers);
		feedUpdates(d.vupdates);

		updSetState(0, d.vdate.v, updQts, d.vseq.v);
	} break;

	case mtpc_updateShort: {
		const MTPDupdateShort &d(updates.c_updateShort());

		feedUpdate(d.vupdate);

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updateShortMessage: {
		const MTPDupdateShortMessage &d(updates.c_updateShortMessage());
		if (!App::userLoaded(d.vuser_id.v) || (d.has_fwd_from_id() && !App::userLoaded(d.vfwd_from_id.v))) {
			MTP_LOG(0, ("getDifference { good - getting user for updateShortMessage }"));
			return getDifference();
		}
		if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
			_byPtsUpdates.insert(ptsKey(SkippedUpdates), updates);
			return;
		}
		bool out = (d.vflags.v & MTPDmessage_flag_out);
		HistoryItem *item = App::histories().addToBack(MTP_message(d.vflags, d.vid, out ? MTP_int(MTP::authedId()) : d.vuser_id, MTP_peerUser(out ? d.vuser_id : MTP_int(MTP::authedId())), d.vfwd_from_id, d.vfwd_date, d.vreply_to_msg_id, d.vdate, d.vmessage, MTP_messageMediaEmpty()));
		if (item) {
			history.peerMessagesUpdated(item->history()->peer->id);
		}

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updateShortChatMessage: {
		const MTPDupdateShortChatMessage &d(updates.c_updateShortChatMessage());
		if (!App::chatLoaded(d.vchat_id.v) || !App::userLoaded(d.vfrom_id.v) || (d.has_fwd_from_id() && !App::userLoaded(d.vfwd_from_id.v))) {
			MTP_LOG(0, ("getDifference { good - getting user for updateShortChatMessage }"));
			return getDifference();
		}
		if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
			_byPtsUpdates.insert(ptsKey(SkippedUpdates), updates);
			return;
		}
		HistoryItem *item = App::histories().addToBack(MTP_message(d.vflags, d.vid, d.vfrom_id, MTP_peerChat(d.vchat_id), d.vfwd_from_id, d.vfwd_date, d.vreply_to_msg_id, d.vdate, d.vmessage, MTP_messageMediaEmpty()));
		if (item) {
			history.peerMessagesUpdated(item->history()->peer->id);
		}

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updatesTooLong: {
		MTP_LOG(0, ("getDifference { good - updatesTooLong received }"));
		return getDifference();
	} break;
	}
}

void MainWidget::feedUpdate(const MTPUpdate &update) {
	if (!MTP::authedId()) return;

	switch (update.type()) {
	case mtpc_updateNewMessage: {
		const MTPDupdateNewMessage &d(update.c_updateNewMessage());
		if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
			_byPtsUpdate.insert(ptsKey(SkippedUpdate), update);
			return;
		}
		HistoryItem *item = App::histories().addToBack(d.vmessage);
		if (item) {
			history.peerMessagesUpdated(item->history()->peer->id);
		}
	} break;

	case mtpc_updateMessageID: {
		const MTPDupdateMessageID &d(update.c_updateMessageID());
		MsgId msg = App::histItemByRandom(d.vrandom_id.v);
		if (msg) {
			HistoryItem *msgRow = App::histItemById(msg);
			if (msgRow) {
				App::historyUnregItem(msgRow);
				History *h = msgRow->history();
				for (int32 i = 0; i < OverviewCount; ++i) {
					History::MediaOverviewIds::iterator j = h->_overviewIds[i].find(msgRow->id);
					if (j != h->_overviewIds[i].cend()) {
						h->_overviewIds[i].erase(j);
						if (h->_overviewIds[i].constFind(d.vid.v) == h->_overviewIds[i].cend()) {
							h->_overviewIds[i].insert(d.vid.v, NullType());
							for (int32 k = 0, l = h->_overview[i].size(); k != l; ++k) {
								if (h->_overview[i].at(k) == msgRow->id) {
									h->_overview[i][k] = d.vid.v;
									break;
								}
							}
						}
					}
				}
				if (App::wnd()) App::wnd()->changingMsgId(msgRow, d.vid.v);
				msgRow->id = d.vid.v;
				if (!App::historyRegItem(msgRow)) {
					msgUpdated(h->peer->id, msgRow);
				} else {
					msgRow->destroy();
					history.peerMessagesUpdated();
				}
			}
			App::historyUnregRandom(d.vrandom_id.v);
		}
	} break;

	case mtpc_updateReadMessages: {
		const MTPDupdateReadMessages &d(update.c_updateReadMessages());
		if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
			_byPtsUpdate.insert(ptsKey(SkippedUpdate), update);
			return;
		}
		App::feedWereRead(d.vmessages.c_vector().v);
	} break;

	case mtpc_updateReadHistoryInbox: {
		const MTPDupdateReadHistoryInbox &d(update.c_updateReadHistoryInbox());
		if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
			_byPtsUpdate.insert(ptsKey(SkippedUpdate), update);
			return;
		}
		App::feedInboxRead(App::peerFromMTP(d.vpeer), d.vmax_id.v);
	} break;

	case mtpc_updateReadHistoryOutbox: {
		const MTPDupdateReadHistoryOutbox &d(update.c_updateReadHistoryOutbox());
		if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
			_byPtsUpdate.insert(ptsKey(SkippedUpdate), update);
			return;
		}
		PeerId peer = App::peerFromMTP(d.vpeer);
		App::feedOutboxRead(peer, d.vmax_id.v);
		if (history.peer() && history.peer()->id == peer) history.update();
	} break;

	case mtpc_updateWebPage: {
		const MTPDupdateWebPage &d(update.c_updateWebPage());
		App::feedWebPage(d.vwebpage);
		history.updatePreview();
	} break;

	case mtpc_updateDeleteMessages: {
		const MTPDupdateDeleteMessages &d(update.c_updateDeleteMessages());
		if (!updPtsUpdated(d.vpts.v, d.vpts_count.v)) {
			_byPtsUpdate.insert(ptsKey(SkippedUpdate), update);
			return;
		}
		App::feedWereDeleted(d.vmessages.c_vector().v);
		history.peerMessagesUpdated();
	} break;

	case mtpc_updateUserTyping: {
		const MTPDupdateUserTyping &d(update.c_updateUserTyping());
		History *history = App::historyLoaded(App::peerFromUser(d.vuser_id));
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (history && user) {
			if (d.vaction.type() == mtpc_sendMessageTypingAction) {
				App::histories().regTyping(history, user);
			} else if (d.vaction.type() == mtpc_sendMessageCancelAction) {
				history->unregTyping(user);
			}
		}
	} break;

	case mtpc_updateChatUserTyping: {
		const MTPDupdateChatUserTyping &d(update.c_updateChatUserTyping());
		History *history = App::historyLoaded(App::peerFromChat(d.vchat_id));
		UserData *user = (d.vuser_id.v == MTP::authedId()) ? 0 : App::userLoaded(d.vuser_id.v);
		if (history && user) {
			App::histories().regTyping(history, user);
		}
	} break;

	case mtpc_updateChatParticipants: {
		const MTPDupdateChatParticipants &d(update.c_updateChatParticipants());
		App::feedParticipants(d.vparticipants);
	} break;

	case mtpc_updateChatParticipantAdd: {
		const MTPDupdateChatParticipantAdd &d(update.c_updateChatParticipantAdd());
		App::feedParticipantAdd(d);
	} break;

	case mtpc_updateChatParticipantDelete: {
		const MTPDupdateChatParticipantDelete &d(update.c_updateChatParticipantDelete());
		App::feedParticipantDelete(d);
	} break;

	case mtpc_updateUserStatus: {
		const MTPDupdateUserStatus &d(update.c_updateUserStatus());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			switch (d.vstatus.type()) {
			case mtpc_userStatusEmpty: user->onlineTill = 0; break;
			case mtpc_userStatusRecently:
				if (user->onlineTill > -10) { // don't modify pseudo-online
					user->onlineTill = -2;
				}
			break;
			case mtpc_userStatusLastWeek: user->onlineTill = -3; break;
			case mtpc_userStatusLastMonth: user->onlineTill = -4; break;
			case mtpc_userStatusOffline: user->onlineTill = d.vstatus.c_userStatusOffline().vwas_online.v; break;
			case mtpc_userStatusOnline: user->onlineTill = d.vstatus.c_userStatusOnline().vexpires.v; break;
			}
			if (App::main()) App::main()->peerUpdated(user);
		}
		if (d.vuser_id.v == MTP::authedId()) {
			if (d.vstatus.type() == mtpc_userStatusOffline || d.vstatus.type() == mtpc_userStatusEmpty) {
				updateOnline(true);
				if (d.vstatus.type() == mtpc_userStatusOffline) {
					cSetOtherOnline(d.vstatus.c_userStatusOffline().vwas_online.v);
				}
			} else if (d.vstatus.type() == mtpc_userStatusOnline) {
				cSetOtherOnline(d.vstatus.c_userStatusOnline().vexpires.v);
			}
		}
	} break;

	case mtpc_updateUserName: {
		const MTPDupdateUserName &d(update.c_updateUserName());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			if (user->contact <= 0) {
				user->setName(textOneLine(qs(d.vfirst_name)), textOneLine(qs(d.vlast_name)), user->nameOrPhone, textOneLine(qs(d.vusername)));
			} else {
				user->setName(textOneLine(user->firstName), textOneLine(user->lastName), user->nameOrPhone, textOneLine(qs(d.vusername)));
			}
			if (App::main()) App::main()->peerUpdated(user);
		}
	} break;

	case mtpc_updateUserPhoto: {
		const MTPDupdateUserPhoto &d(update.c_updateUserPhoto());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			user->setPhoto(d.vphoto);
			user->photo->load();
			if (d.vprevious.v) {
				user->photosCount = -1;
				user->photos.clear();
			} else {
				if (user->photoId) {
					if (user->photosCount > 0) ++user->photosCount;
					user->photos.push_front(App::photo(user->photoId));
				} else {
					user->photosCount = -1;
					user->photos.clear();
				}
			}
			if (App::main()) App::main()->peerUpdated(user);
			if (App::wnd()) App::wnd()->mediaOverviewUpdated(user);
		}
	} break;

	case mtpc_updateContactRegistered: {
		const MTPDupdateContactRegistered &d(update.c_updateContactRegistered());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			if (App::history(user->id)->loadedAtBottom()) {
				App::history(user->id)->addToBackService(clientMsgId(), date(d.vdate), lng_action_user_registered(lt_from, user->name), MTPDmessage_flag_unread);
			}
		}
	} break;

	case mtpc_updateContactLink: {
		const MTPDupdateContactLink &d(update.c_updateContactLink());
		App::feedUserLink(d.vuser_id, d.vmy_link, d.vforeign_link);
	} break;

	case mtpc_updateNotifySettings: {
		const MTPDupdateNotifySettings &d(update.c_updateNotifySettings());
		applyNotifySetting(d.vpeer, d.vnotify_settings);
	} break;

	case mtpc_updateDcOptions: {
		const MTPDupdateDcOptions &d(update.c_updateDcOptions());
		MTP::updateDcOptions(d.vdc_options.c_vector().v);
	} break;

	case mtpc_updateUserPhone: {
		const MTPDupdateUserPhone &d(update.c_updateUserPhone());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			user->setPhone(qs(d.vphone));
			user->setName(user->firstName, user->lastName, (user->contact || isServiceUser(user->id) || user->phone.isEmpty()) ? QString() : App::formatPhone(user->phone), user->username);
			if (App::main()) App::main()->peerUpdated(user);
		}
	} break;

	case mtpc_updateNewGeoChatMessage: {
		const MTPDupdateNewGeoChatMessage &d(update.c_updateNewGeoChatMessage());
	} break;

	case mtpc_updateNewEncryptedMessage: {
		const MTPDupdateNewEncryptedMessage &d(update.c_updateNewEncryptedMessage());
	} break;

	case mtpc_updateEncryptedChatTyping: {
		const MTPDupdateEncryptedChatTyping &d(update.c_updateEncryptedChatTyping());
	} break;

	case mtpc_updateEncryption: {
		const MTPDupdateEncryption &d(update.c_updateEncryption());
	} break;

	case mtpc_updateEncryptedMessagesRead: {
		const MTPDupdateEncryptedMessagesRead &d(update.c_updateEncryptedMessagesRead());
	} break;

	case mtpc_updateUserBlocked: {
		const MTPDupdateUserBlocked &d(update.c_updateUserBlocked());
	} break;

	case mtpc_updateNewAuthorization: {
		const MTPDupdateNewAuthorization &d(update.c_updateNewAuthorization());
		QDateTime datetime = date(d.vdate);

		QString name = App::self()->firstName;
		QString day = langDayOfWeekFull(datetime.date()), date = langDayOfMonth(datetime.date()), time = datetime.time().toString(cTimeFormat());
		QString device = qs(d.vdevice), location = qs(d.vlocation);
		LangString text = lng_new_authorization(lt_name, App::self()->firstName, lt_day, day, lt_date, date, lt_time, time, lt_device, device, lt_location, location);
		App::wnd()->serviceNotification(text);

		emit App::wnd()->newAuthorization();
	} break;

	case mtpc_updateServiceNotification: {
		const MTPDupdateServiceNotification &d(update.c_updateServiceNotification());
		if (d.vpopup.v) {
			App::wnd()->showLayer(new ConfirmBox(qs(d.vmessage), true));
			App::wnd()->serviceNotification(qs(d.vmessage), false, d.vmedia);
		} else {
			App::wnd()->serviceNotification(qs(d.vmessage), true, d.vmedia);
		}
	} break;

	case mtpc_updatePrivacy: {
		const MTPDupdatePrivacy &d(update.c_updatePrivacy());
	} break;
	}
}

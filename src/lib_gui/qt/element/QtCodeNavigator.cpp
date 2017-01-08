#include "qt/element/QtCodeNavigator.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "data/location/TokenLocation.h"
#include "data/location/TokenLocationCollection.h"
#include "data/location/TokenLocationFile.h"
#include "utility/logging/logging.h"
#include "utility/messaging/type/MessageCodeViewExpandedInitialFiles.h"
#include "utility/messaging/type/MessageScrollCode.h"
#include "utility/messaging/type/MessageShowReference.h"

#include "settings/ApplicationSettings.h"
#include "qt/element/QtCodeFile.h"
#include "qt/element/QtCodeSnippet.h"
#include "qt/utility/utilityQt.h"

QtCodeNavigator::QtCodeNavigator(QWidget* parent)
	: QWidget(parent)
	, m_mode(MODE_NONE)
	, m_value(0)
	, m_refIndex(0)
	, m_singleHasNewFile(false)
{
	QVBoxLayout* layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setAlignment(Qt::AlignTop);
	setLayout(layout);

	{
		QWidget* navigation = new QWidget();
		navigation->setObjectName("code_navigation");

		QHBoxLayout* navLayout = new QHBoxLayout();
		navLayout->setSpacing(2);
		navLayout->setContentsMargins(7, 7, 7, 7);


		m_prevButton = new QPushButton("<");
		m_nextButton = new QPushButton(">");

		m_prevButton->setObjectName("reference_button_previous");
		m_nextButton->setObjectName("reference_button_next");

		m_prevButton->setToolTip("previous reference");
		m_nextButton->setToolTip("next reference");

		navLayout->addWidget(m_prevButton);
		navLayout->addWidget(m_nextButton);

		connect(m_prevButton, SIGNAL(clicked()), this, SLOT(previousReference()));
		connect(m_nextButton, SIGNAL(clicked()), this, SLOT(nextReference()));


		m_refLabel = new QLabel("0/0 references");
		m_refLabel->setObjectName("references_label");
		navLayout->addWidget(m_refLabel);

		navLayout->addStretch();


		m_listButton = new QPushButton("list");
		m_fileButton = new QPushButton("file");

		m_listButton->setObjectName("mode_button_list");
		m_fileButton->setObjectName("mode_button_single");

		m_listButton->setCheckable(true);
		m_fileButton->setCheckable(true);

		navLayout->addWidget(m_listButton);
		navLayout->addWidget(m_fileButton);

		connect(m_listButton, SIGNAL(clicked()), this, SLOT(setModeList()));
		connect(m_fileButton, SIGNAL(clicked()), this, SLOT(setModeSingle()));


		navigation->setLayout(navLayout);
		layout->addWidget(navigation);
	}

	m_list = new QtCodeFileList(this);
	layout->addWidget(m_list);

	m_single = new QtCodeFileSingle(this);
	layout->addWidget(m_single);

	if (ApplicationSettings::getInstance()->getCodeViewModeSingle())
	{
		setModeSingle();
	}
	else
	{
		setModeList();
	}

	connect(this, SIGNAL(scrollRequest()), this, SLOT(handleScrollRequest()), Qt::QueuedConnection);
}

QtCodeNavigator::~QtCodeNavigator()
{
}

void QtCodeNavigator::addCodeSnippet(const CodeSnippetParams& params, bool insert)
{
	if (params.reduced)
	{
		m_list->addCodeSnippet(params, insert);
		m_single->addCodeSnippet(params, insert);
	}
	else
	{
		m_current->addCodeSnippet(params, insert);
	}
}

void QtCodeNavigator::addFile(std::shared_ptr<TokenLocationFile> locationFile, int refCount, TimePoint modificationTime)
{
	m_list->addFile(locationFile->getFilePath(), locationFile->isWholeCopy, refCount, modificationTime);

	if (locationFile->isWholeCopy)
	{
		Reference ref;
		ref.filePath = locationFile->getFilePath();
		ref.tokenId = 0;
		ref.locationId = 0;
		ref.locationType = LOCATION_TOKEN;

		m_references.push_back(ref);
	}
	else
	{
		locationFile->forEachStartTokenLocation(
			[&](TokenLocation* location)
			{
				if (!location->isScopeTokenLocation())
				{
					Reference ref;
					ref.filePath = location->getFilePath();
					ref.tokenId = location->getTokenId();
					ref.locationId = location->getId();
					ref.locationType = location->getType();

					m_references.push_back(ref);
				}
			}
		);
	}
}

void QtCodeNavigator::addedFiles()
{
	if (m_references.size() && m_references[0].locationType != LOCATION_TOKEN)
	{
		clearCaches();
	}
}

void QtCodeNavigator::clearCodeSnippets()
{
	m_list->clear();

	m_currentActiveTokenIds.clear();
	m_activeTokenIds.clear();
	m_activeLocalSymbolIds.clear();
	m_focusedTokenIds.clear();
	m_errorInfos.clear();

	if (m_references.size() && m_references[0].locationType != LOCATION_TOKEN)
	{
		clearCaches();
	}

	m_references.clear();
	m_refIndex = 0;

	m_singleHasNewFile = false;
}

void QtCodeNavigator::clearCaches()
{
	m_single->clearCache();
}

const std::vector<Id>& QtCodeNavigator::getCurrentActiveTokenIds() const
{
	return m_currentActiveTokenIds;
}

void QtCodeNavigator::setCurrentActiveTokenIds(const std::vector<Id>& currentActiveTokenIds)
{
	m_currentActiveTokenIds = currentActiveTokenIds;
	m_currentActiveLocationIds.clear();
}

const std::vector<Id>& QtCodeNavigator::getCurrentActiveLocationIds() const
{
	return m_currentActiveLocationIds;
}

void QtCodeNavigator::setCurrentActiveLocationIds(const std::vector<Id>& currentActiveLocationIds)
{
	m_currentActiveLocationIds = currentActiveLocationIds;
	m_currentActiveTokenIds.clear();
}

const std::vector<Id>& QtCodeNavigator::getActiveTokenIds() const
{
	return m_activeTokenIds;
}

void QtCodeNavigator::setActiveTokenIds(const std::vector<Id>& activeTokenIds)
{
	setCurrentActiveTokenIds(activeTokenIds);

	m_activeTokenIds = activeTokenIds;
	m_activeLocalSymbolIds.clear();
}

const std::vector<Id>& QtCodeNavigator::getActiveLocalSymbolIds() const
{
	return m_activeLocalSymbolIds;
}

void QtCodeNavigator::setActiveLocalSymbolIds(const std::vector<Id>& activeLocalSymbolIds)
{
	m_activeLocalSymbolIds = activeLocalSymbolIds;
}

const std::vector<Id>& QtCodeNavigator::getFocusedTokenIds() const
{
	return m_focusedTokenIds;
}

void QtCodeNavigator::setFocusedTokenIds(const std::vector<Id>& focusedTokenIds)
{
	m_focusedTokenIds = focusedTokenIds;
}

std::string QtCodeNavigator::getErrorMessageForId(Id errorId) const
{
	std::map<Id, ErrorInfo>::const_iterator it = m_errorInfos.find(errorId);

	if (it != m_errorInfos.end())
	{
		return it->second.message;
	}

	return "";
}

void QtCodeNavigator::setErrorInfos(const std::vector<ErrorInfo>& errorInfos)
{
	m_errorInfos.clear();

	for (const ErrorInfo& info : errorInfos)
	{
		m_errorInfos.emplace(info.id, info);
	}
}

bool QtCodeNavigator::hasErrors() const
{
	return m_errorInfos.size() > 0;
}

size_t QtCodeNavigator::getFatalErrorCountForFile(const FilePath& filePath) const
{
	size_t fatalErrorCount = 0;
	for (const std::pair<Id, ErrorInfo>& p : m_errorInfos)
	{
		const ErrorInfo& error = p.second;
		if (error.filePath == filePath && error.fatal)
		{
			fatalErrorCount++;
		}
	}
	return fatalErrorCount;
}

void QtCodeNavigator::showActiveSnippet(
	const std::vector<Id>& activeTokenIds, std::shared_ptr<TokenLocationCollection> collection, bool scrollTo)
{
	if (activeTokenIds.size() != 1)
	{
		LOG_ERROR("Number of requested token ids to show is not 1.");
		return;
	}

	Id tokenId = activeTokenIds[0];

	std::vector<Id> locationIds;

	Reference firstReference;
	std::set<FilePath> filePathsToExpand;

	std::map<FilePath, size_t> filePathOrder;

	for (const Reference& ref : m_references)
	{
		if (ref.tokenId == tokenId)
		{
			locationIds.push_back(ref.locationId);

			if (!firstReference.tokenId)
			{
				firstReference = ref;
			}
		}

		filePathOrder.emplace(ref.filePath, filePathOrder.size());
	}

	if (!locationIds.size())
	{
		collection->forEachTokenLocation(
			[&](TokenLocation* location)
			{
				if (location->getTokenId() != tokenId)
				{
					return;
				}

				locationIds.push_back(location->getId());
				filePathsToExpand.insert(location->getFilePath());

				if (!firstReference.tokenId || filePathOrder[location->getFilePath()] < filePathOrder[firstReference.filePath])
				{
					firstReference.tokenId = location->getTokenId();
					firstReference.locationId = location->getId();
					firstReference.filePath = location->getFilePath();
				}
			}
		);
	}

	setCurrentActiveLocationIds(locationIds);
	updateFiles();

	if (m_mode == MODE_LIST)
	{
		for (const FilePath& filePath : filePathsToExpand)
		{
			m_list->requestFileContent(filePath);
		}
	}

	if (firstReference.tokenId)
	{
		requestScroll(firstReference.filePath, 0, firstReference.locationId, true, false);
		emit scrollRequest();

		m_refIndex = 0;
		updateRefLabel();
	}
}

void QtCodeNavigator::focusTokenIds(const std::vector<Id>& focusedTokenIds)
{
	setFocusedTokenIds(focusedTokenIds);
	updateFiles();
}

void QtCodeNavigator::defocusTokenIds()
{
	setFocusedTokenIds(std::vector<Id>());
	updateFiles();
}

void QtCodeNavigator::setFileMinimized(const FilePath path)
{
	m_list->setFileMinimized(path);
}

void QtCodeNavigator::setFileSnippets(const FilePath path)
{
	m_list->setFileSnippets(path);
}

void QtCodeNavigator::setFileMaximized(const FilePath path)
{
	m_list->setFileMaximized(path);
}

void QtCodeNavigator::setupFiles()
{
	if (m_mode == MODE_LIST)
	{
		std::set<FilePath> filePathsToExpand;
		for (const Reference& ref : m_references)
		{
			if (filePathsToExpand.find(ref.filePath) == filePathsToExpand.end())
			{
				m_list->requestFileContent(ref.filePath);
				filePathsToExpand.insert(ref.filePath);

				if (filePathsToExpand.size() >= 3)
				{
					break;
				}
			}
		}

		if (filePathsToExpand.size())
		{
			MessageCodeViewExpandedInitialFiles(m_refIndex != 0).dispatch();
		}
	}
	else if (m_references.size())
	{
		m_singleHasNewFile = (m_single->getCurrentFilePath() != m_references.front().filePath);
		m_single->requestFileContent(m_references.front().filePath);
		MessageCodeViewExpandedInitialFiles(true).dispatch();
	}

	if (m_refIndex == 0)
	{
		updateRefLabel();
	}
}

void QtCodeNavigator::updateFiles()
{
	m_current->updateFiles();
}

void QtCodeNavigator::showContents()
{
	m_current->showContents();
}

void QtCodeNavigator::scrollToValue(int value, bool inListMode)
{
	if ((m_mode == MODE_LIST) == inListMode)
	{
		m_value = value;
		QTimer::singleShot(1000, this, SLOT(setValue()));
	}
}

void QtCodeNavigator::scrollToLine(const FilePath& filePath, unsigned int line)
{
	requestScroll(filePath, line, 0, false, false);
}

void QtCodeNavigator::scrollToDefinition()
{
	if (m_refIndex != 0)
	{
		showCurrentReference(false);
		return;
	}

	if (!m_activeTokenIds.size())
	{
		return;
	}

	Id tokenId = m_activeTokenIds[0]; // The first active tokenId is the one of the active symbol itself.

	if (m_mode == MODE_LIST)
	{
		std::pair<QtCodeSnippet*, uint> result = m_list->getFirstSnippetWithActiveLocation(tokenId);
		if (result.first != nullptr)
		{
			requestScroll(result.first->getFile()->getFilePath(), result.second, 0, false, true);
			emit scrollRequest();
		}
	}
	else
	{
		Id locationId = m_single->getLocationIdOfFirstActiveLocationOfTokenId(tokenId);
		if (locationId)
		{
			for (size_t i = 0; i < m_references.size(); i++)
			{
				if (m_references[i].locationId == locationId)
				{
					m_refIndex = i + 1;
					showCurrentReference(false);
				}
			}
		}
		else if (m_references.size())
		{
			nextReference(false);
		}
	}

	m_singleHasNewFile = false;
}

void QtCodeNavigator::scrollToSnippetIfRequested()
{
	emit scrollRequest();
}

void QtCodeNavigator::requestScroll(const FilePath& filePath, uint lineNumber, Id locationId, bool animated, bool onTop)
{
	ScrollRequest req;
	req.filePath = filePath;
	req.lineNumber = lineNumber;
	req.locationId = locationId;
	req.animated = animated;
	req.onTop = onTop;

	if (m_mode == MODE_SINGLE)
	{
		if (req.lineNumber || m_singleHasNewFile)
		{
			req.animated = false;
		}
		else
		{
			req.animated = (m_single->getCurrentFilePath() == filePath);
		}
	}

	if (m_scrollRequest.filePath.empty())
	{
		m_scrollRequest = req;
	}

	m_singleHasNewFile = false;
}

void QtCodeNavigator::handleScrollRequest()
{
	const ScrollRequest& req = m_scrollRequest;
	if (req.filePath.empty())
	{
		return;
	}

	bool done = m_current->requestScroll(req.filePath, req.lineNumber, req.locationId, req.animated, req.onTop);
	if (done)
	{
		m_scrollRequest = ScrollRequest();
	}
}

void QtCodeNavigator::scrolled(int value)
{
	MessageScrollCode(value, m_mode == MODE_LIST).dispatch();
}

void QtCodeNavigator::setValue()
{
	QAbstractScrollArea* area = m_current->getScrollArea();

	if (area)
	{
		area->verticalScrollBar()->setValue(m_value);
	}
}

void QtCodeNavigator::previousReference(bool fromUI)
{
	if (!m_references.size())
	{
		return;
	}

	if (m_refIndex < 2)
	{
		m_refIndex = m_references.size();
	}
	else
	{
		m_refIndex--;
	}

	showCurrentReference(fromUI);
}

void QtCodeNavigator::nextReference(bool fromUI)
{
	if (!m_references.size())
	{
		return;
	}

	m_refIndex++;

	if (m_refIndex == m_references.size() + 1)
	{
		m_refIndex = 1;
	}

	showCurrentReference(fromUI);
}

void QtCodeNavigator::setModeList()
{
	setMode(MODE_LIST);
}

void QtCodeNavigator::setModeSingle()
{
	setMode(MODE_SINGLE);
}

void QtCodeNavigator::setMode(Mode mode)
{
	if (m_mode == mode)
	{
		return;
	}

	m_mode = mode;

	m_listButton->setChecked(mode == MODE_LIST);
	m_fileButton->setChecked(mode == MODE_SINGLE);

	switch (mode)
	{
		case MODE_SINGLE:
			m_list->hide();
			m_single->show();
			m_current = m_single;
			break;
		case MODE_LIST:
			m_single->hide();
			m_list->show();
			m_current = m_list;
			break;
		default:
			LOG_ERROR("Wrong mode set in code navigator");
			return;
	}

	ApplicationSettings::getInstance()->setCodeViewModeSingle(m_mode == MODE_SINGLE);
	ApplicationSettings::getInstance()->save();

	setupFiles();
	showContents();
}

void QtCodeNavigator::showCurrentReference(bool fromUI)
{
	const Reference& ref = m_references[m_refIndex - 1];

	setCurrentActiveLocationIds(std::vector<Id>(1, ref.locationId));
	updateFiles();

	requestScroll(ref.filePath, 0, ref.locationId, fromUI, false);
	emit scrollRequest();

	updateRefLabel();

	if (fromUI)
	{
		MessageShowReference(m_refIndex - 1, ref.tokenId, ref.locationId).dispatch();
	}
}

void QtCodeNavigator::updateRefLabel()
{
	size_t n = m_references.size();
	size_t t = m_refIndex;

	if (t)
	{
		m_refLabel->setText(QString::number(t) + "/" + QString::number(n) + " references");
	}
	else
	{
		m_refLabel->setText(QString::number(n) + " references");
	}

	m_prevButton->setEnabled(n > 1);
	m_nextButton->setEnabled(n > 1);
}

void QtCodeNavigator::handleMessage(MessageCodeReference* message)
{
	MessageCodeReference::ReferenceType type = message->type;

	m_onQtThread(
		[=]()
		{
			if (type == MessageCodeReference::REFERENCE_PREVIOUS)
			{
				previousReference();
			}
			else if (type == MessageCodeReference::REFERENCE_NEXT)
			{
				nextReference();
			}
		}
	);
}

void QtCodeNavigator::handleMessage(MessageFinishedParsing* message)
{
	m_onQtThread(
		[=]()
		{
			clearCaches();
		}
	);
}

void QtCodeNavigator::handleMessage(MessageSwitchColorScheme* message)
{
	m_onQtThread(
		[=]()
		{
			clearCaches();
		}
	);
}

void QtCodeNavigator::handleMessage(MessageWindowFocus* message)
{
	m_onQtThread(
		[=]()
		{
			m_current->onWindowFocus();
		}
	);
}

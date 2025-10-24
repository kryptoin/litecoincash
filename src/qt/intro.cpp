// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <fs.h>
#include <qt/forms/ui_intro.h>
#include <qt/intro.h>

#include <qt/guiutil.h>

#include <util.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include <cmath>

static const uint64_t GB_BYTES = 1000000000LL;

static const uint64_t BLOCK_CHAIN_SIZE = 14;

static const uint64_t CHAIN_STATE_SIZE = 3;

static uint64_t requiredSpace;

class FreespaceChecker : public QObject {
  Q_OBJECT

public:
  explicit FreespaceChecker(Intro *intro);

  enum Status { ST_OK, ST_ERROR };

public Q_SLOTS:
  void check();

Q_SIGNALS:
  void reply(int status, const QString &message, quint64 available);

private:
  Intro *intro;
};

#include <qt/intro.moc>

FreespaceChecker::FreespaceChecker(Intro *_intro) { this->intro = _intro; }

void FreespaceChecker::check() {
  QString dataDirStr = intro->getPathToCheck();
  fs::path dataDir = GUIUtil::qstringToBoostPath(dataDirStr);
  uint64_t freeBytesAvailable = 0;
  int replyStatus = ST_OK;
  QString replyMessage = tr("A new data directory will be created.");

  fs::path parentDir = dataDir;
  fs::path parentDirOld = fs::path();
  while (parentDir.has_parent_path() && !fs::exists(parentDir)) {
    parentDir = parentDir.parent_path();

    if (parentDirOld == parentDir)
      break;

    parentDirOld = parentDir;
  }

  try {
    freeBytesAvailable = fs::space(parentDir).available;
    if (fs::exists(dataDir)) {
      if (fs::is_directory(dataDir)) {
        QString separator =
            "<code>" + QDir::toNativeSeparators("/") + tr("name") + "</code>";
        replyStatus = ST_OK;
        replyMessage = tr("Directory already exists. Add %1 if you intend to "
                          "create a new directory here.")
                           .arg(separator);
      } else {
        replyStatus = ST_ERROR;
        replyMessage = tr("Path already exists, and is not a directory.");
      }
    }
  } catch (const fs::filesystem_error &) {
    replyStatus = ST_ERROR;
    replyMessage = tr("Cannot create data directory here.");
  }
  Q_EMIT reply(replyStatus, replyMessage, freeBytesAvailable);
}

Intro::Intro(QWidget *parent)
    : QDialog(parent), ui(new Ui::Intro), thread(0), signalled(false) {
  ui->setupUi(this);
  ui->welcomeLabel->setText(ui->welcomeLabel->text().arg(tr(PACKAGE_NAME)));
  ui->storageLabel->setText(ui->storageLabel->text().arg(tr(PACKAGE_NAME)));

  ui->lblExplanation1->setText(ui->lblExplanation1->text()
                                   .arg(tr(PACKAGE_NAME))
                                   .arg(BLOCK_CHAIN_SIZE)
                                   .arg(2011)

                                   .arg(tr("Litecoin"))

  );
  ui->lblExplanation2->setText(
      ui->lblExplanation2->text().arg(tr(PACKAGE_NAME)));

  uint64_t pruneTarget = std::max<int64_t>(0, gArgs.GetArg("-prune", 0));
  requiredSpace = BLOCK_CHAIN_SIZE;
  QString storageRequiresMsg =
      tr("At least %1 GB of data will be stored in this directory, and it will "
         "grow over time.");
  if (pruneTarget) {
    uint64_t prunedGBs = std::ceil(pruneTarget * 1024 * 1024.0 / GB_BYTES);
    if (prunedGBs <= requiredSpace) {
      requiredSpace = prunedGBs;
      storageRequiresMsg =
          tr("Approximately %1 GB of data will be stored in this directory.");
    }
    ui->lblExplanation3->setVisible(true);
  } else {
    ui->lblExplanation3->setVisible(false);
  }
  requiredSpace += CHAIN_STATE_SIZE;
  ui->sizeWarningLabel->setText(
      tr("%1 will download and store a copy of the LitecoinCash block chain.")
          .arg(tr(PACKAGE_NAME)) +
      " " + storageRequiresMsg.arg(requiredSpace) + " " +
      tr("The wallet will also be stored in this directory."));
  startThread();
}

Intro::~Intro() {
  delete ui;

  Q_EMIT stopThread();
  thread->wait();
}

QString Intro::getDataDirectory() { return ui->dataDirectory->text(); }

void Intro::setDataDirectory(const QString &dataDir) {
  ui->dataDirectory->setText(dataDir);
  if (dataDir == getDefaultDataDirectory()) {
    ui->dataDirDefault->setChecked(true);
    ui->dataDirectory->setEnabled(false);
    ui->ellipsisButton->setEnabled(false);
  } else {
    ui->dataDirCustom->setChecked(true);
    ui->dataDirectory->setEnabled(true);
    ui->ellipsisButton->setEnabled(true);
  }
}

QString Intro::getDefaultDataDirectory() {
  return GUIUtil::boostPathToQString(GetDefaultDataDir());
}

bool Intro::pickDataDirectory() {
  QSettings settings;

  if (!gArgs.GetArg("-datadir", "").empty())
    return true;

  QString dataDir = getDefaultDataDirectory();

  dataDir = settings.value("strDataDir", dataDir).toString();

  if (!fs::exists(GUIUtil::qstringToBoostPath(dataDir)) ||
      gArgs.GetBoolArg("-choosedatadir", DEFAULT_CHOOSE_DATADIR) ||
      settings.value("fReset", false).toBool() ||
      gArgs.GetBoolArg("-resetguisettings", false)) {
    Intro intro;
    intro.setDataDirectory(dataDir);
    intro.setWindowIcon(QIcon(":icons/bitcoin"));

    while (true) {
      if (!intro.exec()) {
        return false;
      }
      dataDir = intro.getDataDirectory();
      try {
        if (TryCreateDirectories(GUIUtil::qstringToBoostPath(dataDir))) {
          TryCreateDirectories(GUIUtil::qstringToBoostPath(dataDir) /
                               "wallets");
        }
        break;
      } catch (const fs::filesystem_error &) {
        QMessageBox::critical(
            0, tr(PACKAGE_NAME),
            tr("Error: Specified data directory \"%1\" cannot be created.")
                .arg(dataDir));
      }
    }

    settings.setValue("strDataDir", dataDir);
    settings.setValue("fReset", false);
  }

  if (dataDir != getDefaultDataDirectory())
    gArgs.SoftSetArg("-datadir", GUIUtil::qstringToBoostPath(dataDir).string());

  return true;
}

void Intro::setStatus(int status, const QString &message,
                      quint64 bytesAvailable) {
  switch (status) {
  case FreespaceChecker::ST_OK:
    ui->errorMessage->setText(message);
    ui->errorMessage->setStyleSheet("");
    break;
  case FreespaceChecker::ST_ERROR:
    ui->errorMessage->setText(tr("Error") + ": " + message);
    ui->errorMessage->setStyleSheet("QLabel { color: #800000 }");
    break;
  }

  if (status == FreespaceChecker::ST_ERROR) {
    ui->freeSpace->setText("");
  } else {
    QString freeString =
        tr("%n GB of free space available", "", bytesAvailable / GB_BYTES);
    if (bytesAvailable < requiredSpace * GB_BYTES) {
      freeString += " " + tr("(of %n GB needed)", "", requiredSpace);
      ui->freeSpace->setStyleSheet("QLabel { color: #800000 }");
    } else {
      ui->freeSpace->setStyleSheet("");
    }
    ui->freeSpace->setText(freeString + ".");
  }

  ui->buttonBox->button(QDialogButtonBox::Ok)
      ->setEnabled(status != FreespaceChecker::ST_ERROR);
}

void Intro::on_dataDirectory_textChanged(const QString &dataDirStr) {
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
  checkPath(dataDirStr);
}

void Intro::on_ellipsisButton_clicked() {
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
      0, "Choose data directory", ui->dataDirectory->text()));
  if (!dir.isEmpty())
    ui->dataDirectory->setText(dir);
}

void Intro::on_dataDirDefault_clicked() {
  setDataDirectory(getDefaultDataDirectory());
}

void Intro::on_dataDirCustom_clicked() {
  ui->dataDirectory->setEnabled(true);
  ui->ellipsisButton->setEnabled(true);
}

void Intro::startThread() {
  thread = new QThread(this);
  FreespaceChecker *executor = new FreespaceChecker(this);
  executor->moveToThread(thread);

  connect(executor, SIGNAL(reply(int, QString, quint64)), this,
          SLOT(setStatus(int, QString, quint64)));
  connect(this, SIGNAL(requestCheck()), executor, SLOT(check()));

  connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
  connect(this, SIGNAL(stopThread()), thread, SLOT(quit()));

  thread->start();
}

void Intro::checkPath(const QString &dataDir) {
  mutex.lock();
  pathToCheck = dataDir;
  if (!signalled) {
    signalled = true;
    Q_EMIT requestCheck();
  }
  mutex.unlock();
}

QString Intro::getPathToCheck() {
  QString retval;
  mutex.lock();
  retval = pathToCheck;
  signalled = false;

  mutex.unlock();
  return retval;
}

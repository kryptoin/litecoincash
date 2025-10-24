// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PLATFORMSTYLE_H
#define BITCOIN_QT_PLATFORMSTYLE_H

#include <QIcon>
#include <QPixmap>
#include <QString>

class PlatformStyle {
public:
  static const PlatformStyle *instantiate(const QString &platformId);

  const QString &getName() const { return name; }

  bool getImagesOnButtons() const { return imagesOnButtons; }
  bool getUseExtraSpacing() const { return useExtraSpacing; }

  QColor TextColor() const { return textColor; }
  QColor SingleColor() const { return singleColor; }

  QImage SingleColorImage(const QString &filename) const;

  QIcon SingleColorIcon(const QString &filename) const;

  QIcon SingleColorIcon(const QIcon &icon) const;

  QIcon TextColorIcon(const QString &filename) const;

  QIcon TextColorIcon(const QIcon &icon) const;

private:
  PlatformStyle(const QString &name, bool imagesOnButtons, bool colorizeIcons,
                bool useExtraSpacing);

  QString name;
  bool imagesOnButtons;
  bool colorizeIcons;
  bool useExtraSpacing;
  QColor singleColor;
  QColor textColor;
};

#endif

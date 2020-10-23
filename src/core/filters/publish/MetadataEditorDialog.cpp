#include "MetadataEditorDialog.h"
#include "MetadataEditorDialog.moc"

#include <QStandardItemModel>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegExpValidator>
#include <QLabel>

#include "config.h"
#include "version.h"

namespace publish {

QComboBoxDelegate::QComboBoxDelegate(QVector<DocMetaField>& knownValues, QTableWidget *parent)
    : QStyledItemDelegate(parent),
      m_parent(parent),
      m_knownValues(knownValues)
{
}

QStringList getFieldsList(const QAbstractItemModel* model)
{
    QStringList res;
    for (int i = 0; i < model->rowCount(); i++) {
        const QModelIndex idx = model->index(i, 0);
        res.append(model->data(idx).toString());
    }
    return res;
}

bool IsKnownField(const QVector<DocMetaField>& list, const QString& field)
{
    for (const auto& f: list) {
        if (f.code == field) {
            return true;
        }
    }
    return false;
}

QWidget *QComboBoxDelegate::createEditor(QWidget *parent,
                                         const QStyleOptionViewItem &/* option */,
                                         const QModelIndex & index ) const
{
    QComboBox *editor = new QComboBox(parent);
    editor->setFrame(false);
    QString cur_value = m_parent->model()->data(index).toString();
    QStringList values;
    QStringList shown_values = getFieldsList(m_parent->model());

    for (const DocMetaField& f: qAsConst(m_knownValues) ) {
        if (f.name == cur_value ||
                shown_values.indexOf(f.name) == -1) {
            values << f.name;
        }
    }

    editor->addItems(values);
    editor->showPopup();
    return editor;
}

void QComboBoxDelegate::setEditorData(QWidget *editor,
                                      const QModelIndex &index) const
{
    QString value = index.model()->data(index).toString();

    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    int idx = comboBox->findText(value);
    if (idx != -1) {
        comboBox->setCurrentIndex(idx);
    } else {
        comboBox->addItem(value);
        comboBox->setCurrentIndex(comboBox->count()-1);
    }
    m_parent->setItem(index.row(), 1, new QTableWidgetItem()); // reset value column
}

void QComboBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                     const QModelIndex &index) const
{
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    model->setData(index, comboBox->currentText());
}

void QComboBoxDelegate::updateEditorGeometry(QWidget *editor,
                                             const QStyleOptionViewItem &option,
                                             const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}

QVector<DocMetaField> MetadataEditorDialog::m_knownDocMeta =
{
    {QObject::tr("Author"), "author", 1, 0, ""},
    {QObject::tr("Title"), "title", 1, 0, ""},
    {QObject::tr("Publisher"), "publisher", 1, 0, ""},
    {QObject::tr("Year"), "year", 1, 0, ""},
    {QObject::tr("ISBN"), "isbn", 1, 0, ""},
    {QObject::tr("Size"), "size", 0, 0, ""},
    {QObject::tr("Binding"), "binding", 0, 0, ""},
    {QObject::tr("Pages"), "pages", 0, 0, ""},
    {QObject::tr("Price"), "price", 0, 0, ""},
    {QObject::tr("UDC"), "udc", 0, 0, ""},
    {QObject::tr("Language"), "language", 0, 0, ""},
    {QObject::tr("Keywords"), "keywords", 0, 0, ""},
    {QObject::tr("Ex Libris"), "ex-libris", 1, 0, ""},
    {QObject::tr("Software"), "software", 1, 0, APPLICATION_NAME " " VERSION},
    {QObject::tr("Version"), "version", 1, 0, ""},
    {QObject::tr("Notes"), "notes", 1, 0, ""}
};

void addField(QTableWidget* view, const QString& name, const QString& code, const QString& value)
{
    const int row = view->rowCount();
    QTableWidgetItem* item = new QTableWidgetItem(name.isEmpty()? code: name);
    item->setData(Qt::UserRole, code);
    view->setRowCount(row+1);
    view->setItem(row , 0, item);
    if (!value.isEmpty()) {
        item = new QTableWidgetItem(value);
        view->setItem(row , 1, item);
    }
}

void displayDefaultFieldsList(QTableWidget* view, const QVector<DocMetaField>& known_meta)
{
    view->clearContents();
    for (const DocMetaField& f: known_meta ) {
        if (f.is_default) {
            addField(view, f.name, f.code, f.default_value);
        }
    }
}

void displayMetadata(QTableWidget* view, const QMap<QString, QString>& metadata, QVector<DocMetaField>& known_meta)
{
    view->clearContents();
    for (QMap<QString, QString>::const_iterator it = metadata.cbegin();
         it != metadata.cend(); it++) {
        bool found = false;
        for (const DocMetaField& f: qAsConst(known_meta)) {
            if (f.code == it.key()) {
                found = true;
                addField(view, f.name, f.code, it.value());
                break;
            }
        }
        if (!found) {
            addField(view, it.key(), it.key(), it.value());
            known_meta.append({it.key(), it.key(), 0, 0, ""});
        }
    }
}

MetadataEditorDialog::MetadataEditorDialog(const QMap<QString, QString>& metadata, QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);
    m_currentDocMeta = m_knownDocMeta;
    m_delegate = new QComboBoxDelegate(m_currentDocMeta, tvDocumentMeta);
    tvDocumentMeta->setItemDelegateForColumn(0, m_delegate);
    displayMetadata(tvDocumentMeta, metadata, m_currentDocMeta);
}

MetadataEditorDialog::~MetadataEditorDialog()
{
    delete m_delegate;
}

void MetadataEditorDialog::on_btnAdd_clicked()
{
    tvDocumentMeta->setRowCount(tvDocumentMeta->rowCount()+1);
}

void MetadataEditorDialog::on_btnReset_clicked()
{
    displayDefaultFieldsList(tvDocumentMeta, m_currentDocMeta);
}

void MetadataEditorDialog::on_btnRemove_clicked()
{
    const int row = tvDocumentMeta->currentIndex().row();
    if (row > 0) {
        tvDocumentMeta->removeRow(row);
    }
}

void MetadataEditorDialog::on_btnNewField_clicked()
{
    bool ok;
    QString field_id;
    do {
        // editor isn't valid after this line
        QInputDialog dialog(this);
        dialog.setWindowTitle(tr("New metadata field"));
        dialog.setLabelText(tr("Input new metadata field id:"));
        dialog.setInputMode(QInputDialog::TextInput);

        QLineEdit* editor = dialog.findChild<QLineEdit*>();
        if (editor) {
            QFontMetrics metrix(editor->font());
            const int w = metrix.boundingRect(dialog.windowTitle()).width() + 200;
            const QRect r = dialog.geometry();
            if (r.width() < w) {
                dialog.setGeometry(x() + width()/2 - w,
                                   y() + height()/2 - r.height(),
                                   w, r.height());
            }

            QRegularExpression rx("[a-z0-9_-]+"); // lowercase, no space
            QValidator *validator = new QRegularExpressionValidator(rx, this);
            editor->setValidator(validator);
        }

        ok = (dialog.exec() == QDialog::Accepted);

        field_id = field_id.trimmed();
        if (ok && !field_id.isEmpty()) {
            if (IsKnownField(m_currentDocMeta, field_id)) {
                field_id.clear();
                QMessageBox::warning(this, tr("Wrong metadata field"), tr("Field \"%1\" is already exists").arg(field_id));
            }
        }

    } while (ok && field_id.isEmpty());

    if (ok) {
        m_currentDocMeta.append({field_id, field_id, 0, 0, ""});
    }
}

QMap<QString, QString> MetadataEditorDialog::getDefaultMetadata()
{
    QMap<QString, QString> res;
    for (const DocMetaField& f: qAsConst(m_knownDocMeta) ) {
        if (f.is_default && !f.default_value.isEmpty()) {
            res[f.code] = f.default_value;
        }
    }
    return res;
}

QMap<QString, QString> MetadataEditorDialog::getMetadata()
{
    QMap<QString, QString> res;
    const QAbstractItemModel* const model = tvDocumentMeta->model();
    for (int i = 0; i < model->rowCount(); i++ ) {
        const QModelIndex key_index = model->index(i, 0);
        const QModelIndex val_index = model->index(i, 1);
        const QString val = model->data(val_index).toString().trimmed();
        if (!val.isEmpty()) { // empty metadata fields are ignored
            res[model->data(key_index, Qt::UserRole).toString()] = val;
        }
    }
    return res;
}

} // namespace publish


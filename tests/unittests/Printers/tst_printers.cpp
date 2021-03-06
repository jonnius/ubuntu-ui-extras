/*
 * Copyright (C) 2017 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mockbackend.h"

#include "backend/backend.h"
#include "printers/printers.h"

#include <QDebug>
#include <QObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

Q_DECLARE_METATYPE(PrinterBackend*)
Q_DECLARE_METATYPE(QList<QSharedPointer<Printer>>)

class TestPrinters : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testInstantiation_data()
    {
        QTest::addColumn<PrinterBackend*>("backend");

        {
            PrinterBackend* backend = new MockPrinterBackend;
            QTest::newRow("no printers") << backend;
        }
    }
    void testInstantiation()
    {
        QFETCH(PrinterBackend*, backend);
        Printers printers(backend);
    }
    void testAllPrintersFilter_data()
    {
        QTest::addColumn<QStringList>("in");
        QTest::addColumn<QStringList>("out");
        QTest::addColumn<QString>("defaultPrinterName");
        {
            auto in = QStringList({"printer-a", "printer-b"});
            auto out = QStringList({"printer-a", "printer-b"});

            QTest::newRow("no defaults") << in << out << "";
        }
        {
            auto in = QStringList({"printer-a", "printer-b"});
            auto out = QStringList({"printer-b", "printer-a"});
            QTest::newRow("have default") << in << out << "printer-b";
        }
    }
    void testAllPrintersFilter()
    {
        QFETCH(QStringList, in);
        QFETCH(QStringList, out);
        QFETCH(QString, defaultPrinterName);

        MockPrinterBackend* backend = new MockPrinterBackend;
        backend->m_defaultPrinterName = defaultPrinterName;
        Q_FOREACH(auto existingPrinter, in) {
            backend->m_availablePrinterNames << existingPrinter;
        }
        Printers printers(backend);

        auto all = printers.allPrinters();

        QCOMPARE(all->rowCount(), out.size());
        for (int i = 0; i < all->rowCount(); i++) {
            QCOMPARE(
                 all->data(all->index(i, 0)).toString(),
                 out.at(i)
            );
        }
    }
    void testPrinterDrivers()
    {
        QString targetFilter("foo");
        Printers printers(new MockPrinterBackend);
        printers.setDriverFilter(targetFilter);

        DriverModel *drivers = (DriverModel*) printers.drivers();
        QCOMPARE(drivers->filter(), targetFilter);
    }
    void testCreateJob()
    {
        // Setup the backend with a printer
        const QString printerName = QStringLiteral("test-printer");

        MockPrinterBackend *backend = new MockPrinterBackend;
        Printers p(backend);

        MockPrinterBackend *printerBackend = new MockPrinterBackend(printerName);
        auto printer = QSharedPointer<Printer>(new Printer(printerBackend));
        backend->mockPrinterLoaded(printer);

        // Create a job
        PrinterJob *job = p.createJob(printerName);

        // Check it has a printerName
        QCOMPARE(job->printerName(), printerName);
    }
    void testCancelJob()
    {
        MockPrinterBackend *backend = new MockPrinterBackend;
        JobModel *model = new JobModel(backend);
        Printers p(backend);

        // Add one.
        QSharedPointer<PrinterJob> job = QSharedPointer<PrinterJob>(new PrinterJob("test-printer", backend, 1));
        backend->m_jobs << job;
        backend->mockJobCreated("", "", "test-printer", 1, "", true, 1, 1, "", "", 1);

        // Check it was added
        QCOMPARE(model->count(), 1);

        // Setup the spy
        QSignalSpy removeSpy(model, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));

        // Cancel the job
        p.cancelJob(job->printerName(), job->jobId());

        // Check item was removed
        QTRY_COMPARE(removeSpy.count(), 1);

        QList<QVariant> args = removeSpy.at(0);
        QCOMPARE(args.at(1).toInt(), 0);
        QCOMPARE(args.at(2).toInt(), 0);
    }
    void testHoldJob()
    {
        MockPrinterBackend *backend = new MockPrinterBackend;
        JobModel *model = new JobModel(backend);
        Printers p(backend);

        // Add one.
        QSharedPointer<PrinterJob> job = QSharedPointer<PrinterJob>(new PrinterJob("test-printer", backend, 1));
        backend->m_jobs << job;
        backend->mockJobCreated("", "", "test-printer", 1, "", true, 1, static_cast<uint>(PrinterEnum::JobState::Pending), "", "", 1);

        // Check it was added
        QCOMPARE(model->count(), 1);
        QCOMPARE(model->getJob("test-printer", 1)->state(), PrinterEnum::JobState::Pending);

        // Setup the spy
        QSignalSpy dataChangedSpy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));

        // Hold the job
        p.holdJob(job->printerName(), job->jobId());

        // Check item was removed
        QTRY_COMPARE(dataChangedSpy.count(), 1);
        QCOMPARE(model->getJob("test-printer", 1)->state(), PrinterEnum::JobState::Held);
    }
    void testReleaseJob()
    {
        MockPrinterBackend *backend = new MockPrinterBackend;
        JobModel *model = new JobModel(backend);
        Printers p(backend);

        // Add one.
        QSharedPointer<PrinterJob> job = QSharedPointer<PrinterJob>(new PrinterJob("test-printer", backend, 1));
        backend->m_jobs << job;
        backend->mockJobCreated("", "", "test-printer", 1, "", true, 1, 1, "", "", 1);

        p.holdJob(job->printerName(), job->jobId());

        // Check it was added and is in held state
        QCOMPARE(model->count(), 1);
        QCOMPARE(model->getJob("test-printer", 1)->state(), PrinterEnum::JobState::Held);

        // Setup the spy
        QSignalSpy dataChangedSpy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));

        // Release the job
        p.releaseJob(job->printerName(), job->jobId());

        // Check item was removed
        QTRY_COMPARE(dataChangedSpy.count(), 1);
        QCOMPARE(model->getJob("test-printer", 1)->state(), PrinterEnum::JobState::Pending);
    }
    void testPrinterRemove()
    {
        // Load the backend with a printer
        const QString printerName = QStringLiteral("test-printer");

        MockPrinterBackend *backend = new MockPrinterBackend;

        MockPrinterBackend *printerBackend = new MockPrinterBackend(printerName);
        auto printer = QSharedPointer<Printer>(new Printer(printerBackend));

        backend->m_availablePrinterNames << printerName;
        backend->m_availablePrinters << printer;

        Printers printers(backend);
        auto all = printers.allPrinters();

        // Check the initial printer count
        QCOMPARE(all->rowCount(), 1);

        // Setup a spy
        QSignalSpy removeSpy(all, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));

        // Remove the item
        printers.removePrinter(printerName);
        backend->mockPrinterDeleted("", "", printerName, 1, "", true);

        // Check item was removed
        QTRY_COMPARE(removeSpy.count(), 1);

        QList<QVariant> args = removeSpy.at(0);
        QCOMPARE(args.at(1).toInt(), 0);
        QCOMPARE(args.at(2).toInt(), 0);

        QCOMPARE(all->rowCount(), 0);
    }
    void testSetDefault()
    {
        const QString defaultPrinterName = QStringLiteral("my-default-printer");

        MockPrinterBackend *backend = new MockPrinterBackend;
        Printers p(backend);

        QCOMPARE(p.defaultPrinterName(), QString());

        p.setDefaultPrinterName(defaultPrinterName);

        QCOMPARE(p.defaultPrinterName(), defaultPrinterName);
    }
    /* Test that Printers successfully assigns printers to jobs whenever
    they appear, as well as assigning job proxies to printers whenever they
    appear. */
    void testAssignPrinterToJob()
    {
        MockPrinterBackend *backend = new MockPrinterBackend;
        Printers p(backend);

        MockPrinterBackend *printerBackend = new MockPrinterBackend("test-printer");
        auto printer = QSharedPointer<Printer>(new Printer(printerBackend));
        backend->mockPrinterLoaded(printer);

        auto job = QSharedPointer<PrinterJob>(new PrinterJob("test-printer", backend, 1));
        backend->m_jobs << job;

        // Setup the spy
        QSignalSpy jobLoadedSpy(backend, SIGNAL(jobLoaded(QString, int, QMap<QString, QVariant>)));

        // Trigger update.
        backend->mockJobCreated("", "", "test-printer", 1, "", true, 1, 1, "", "", 1);

        QTRY_COMPARE(jobLoadedSpy.count(), 1);

        // Job now has a shared pointer to printer.
        JobModel *model = static_cast<JobModel *>(p.printJobs());

        QCOMPARE(model->getJob(printer->name(), job->jobId())->printer(), printer);
        QCOMPARE(model->getJob(printer->name(), job->jobId())->printer()->name(), printer->name());
    }
    void testSetPrinterJobFilter()
    {
        MockPrinterBackend *backend = new MockPrinterBackend;
        Printers p(backend);

        auto job = QSharedPointer<PrinterJob>(new PrinterJob("test-printer", backend, 1));
        backend->m_jobs << job;
        backend->mockJobCreated("", "", "test-printer", 1, "", true, 1, 1, "", "", 1);

        MockPrinterBackend *printerBackend = new MockPrinterBackend("test-printer");
        auto printer = QSharedPointer<Printer>(new Printer(printerBackend));
        backend->mockPrinterLoaded(printer);

        QCOMPARE(printer->jobs()->rowCount(), 1);

        // Need to also get this through a model.
        auto printerJobs = p.allPrinters()->data(p.allPrinters()->index(0,0), PrinterModel::Roles::JobRole).value<QAbstractItemModel*>();
        QCOMPARE(printerJobs->rowCount(), 1);
    }
    void testLoadPrinter()
    {
        MockPrinterBackend *backend = new MockPrinterBackend;
        Printers p(backend);

        // Harmless to request non-existent one.
        p.loadPrinter("foo");

        // Load a printer and request it.
        MockPrinterBackend *printerBackend = new MockPrinterBackend("printer-a");
        auto printer = QSharedPointer<Printer>(new Printer(printerBackend));
        backend->mockPrinterLoaded(printer);
        p.loadPrinter("printer-a");
        QVERIFY(backend->m_requestedPrinters.contains("printer-a"));
    }
    void testPrintTestPage()
    {
        QStandardPaths::setTestModeEnabled(true);

        MockPrinterBackend *backend = new MockPrinterBackend;
        Printers p(backend);

        // Load a printer and request it.
        MockPrinterBackend *printerBackend = new MockPrinterBackend("printer-a");
        auto printer = QSharedPointer<Printer>(new Printer(printerBackend));
        backend->mockPrinterLoaded(printer);
        p.loadPrinter("printer-a");

        // Set the target url
        auto target = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                             "cups/data/default-testpage.pdf",
                                             QStandardPaths::LocateFile);

        QSignalSpy printFileSpy(printerBackend, SIGNAL(printToFile(QString, QString)));
        p.printTestPage("printer-a");
        QCOMPARE(printFileSpy.count(), 1);
        QList<QVariant> args = printFileSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), target);
    }

    /* Test that a printer is 1) enabled and 2) set to accepting jobs upon
    creation. Also, if the printer is the only printer, make it the default. */
    void testPrinterProvisioning()
    {
        MockPrinterBackend *backend = new MockPrinterBackend;
        Printers p(backend);
        p.addPrinter("printer-a", "some-ppd", "ipp://foo/bar", "", "");

        // Create the printer, and make it appear in the printer model.
        MockPrinterBackend *printerBackend = new MockPrinterBackend("printer-a");
        auto printer = QSharedPointer<Printer>(new Printer(printerBackend));
        backend->mockPrinterLoaded(printer);
        p.loadPrinter("printer-a");

        QVERIFY(backend->enableds.contains("printer-a"));
        QVERIFY(backend->printerOptions["printer-a"].value("AcceptJobs").toBool());
        QCOMPARE(backend->m_defaultPrinterName, (QString) "printer-a");

        p.addPrinter("printer-b", "some-ppd", "ipp://bar/baz", "", "");
        QVERIFY(backend->enableds.contains("printer-b"));
        QVERIFY(backend->printerOptions["printer-b"].value("AcceptJobs").toBool());
        QCOMPARE(backend->m_defaultPrinterName, (QString) "printer-a");
    }
};

QTEST_GUILESS_MAIN(TestPrinters)
#include "tst_printers.moc"

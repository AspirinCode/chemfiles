// Chemfiles, a modern library for chemistry file reading and writing
// Copyright (C) Guillaume Fraux and contributors -- BSD license

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "chemfiles/formats/GRO.hpp"

#include "chemfiles/ErrorFmt.hpp"
#include "chemfiles/File.hpp"
#include "chemfiles/Frame.hpp"
#include "chemfiles/utils.hpp"
#include "chemfiles/warnings.hpp"

using namespace chemfiles;

template<> FormatInfo chemfiles::format_information<GROFormat>() {
    return FormatInfo("GRO").with_extension(".gro").description(
        "GROMACS GRO text format"
    );
}

/// Check the number of digits before the decimal separator to be sure than
/// we can represen them. In case of error, use the given `context` in the error
/// message
static void check_values_size(const Vector3D& values, unsigned width, const std::string& context);
/// Fast-forward the file for one step, returning `false` if the file does
/// not contain one more step.
static bool forward(TextFile& file);

GROFormat::GROFormat(std::string path, File::Mode mode, File::Compression compression)
    : file_(TextFile::open(std::move(path), mode, compression))
{
    while (!file_->eof()) {
        auto position = file_->tellg();
        if (!file_ || position == std::streampos(-1)) {
            throw format_error("IO error while reading '{}' as GRO", path);
        }
        if (forward(*file_)) {
            steps_positions_.push_back(position);
        }
    }
    file_->rewind();
}

size_t GROFormat::nsteps() {
    return steps_positions_.size();
}

void GROFormat::read_step(const size_t step, Frame& frame) {
    assert(step < steps_positions_.size());
    file_->seekg(steps_positions_[step]);
    read(frame);
}

void GROFormat::read(Frame& frame) {
    size_t natoms = 0;
    try {
        frame.set("name", (file_->readline())); // GRO comment line;
        natoms = parse<size_t>(file_->readline());
    } catch (const Error& e) {
        throw format_error("can not read next step as GRO: {}", e.what());
    }

    residues_.clear();
    frame.add_velocities();
    frame.reserve(natoms);
    frame.resize(0);

    for (const auto& line: file_->readlines(natoms)) {

        if (line.length() < 44) {
            throw format_error(
                "GRO Atom line is too small: '{}'", line
            );
        }

        size_t resid = SIZE_MAX;
        try {
            resid = parse<size_t>(line.substr(0, 5));
        } catch (const Error&) {
            // Invalid residue, we'll skip it
        }
        auto resname = trim(line.substr(5, 5));
        auto name = trim(line.substr(10, 5));

        // GRO files store atoms in NM, we need to convert to Angstroms
        auto x = parse<double>(line.substr(20, 8)) * 10;
        auto y = parse<double>(line.substr(28, 8)) * 10;
        auto z = parse<double>(line.substr(36, 8)) * 10;

        if (line.length() >= 68) {
            auto vx = parse<double>(line.substr(44, 8)) * 10;
            auto vy = parse<double>(line.substr(52, 8)) * 10;
            auto vz = parse<double>(line.substr(60, 8)) * 10;

            frame.add_atom(Atom(name),
                Vector3D(x, y, z),
                Vector3D(vx, vy, vz)
            );
        } else {
            frame.add_atom(Atom(name),
                Vector3D(x, y, z)
            );
        }

        if (resid != SIZE_MAX) {
            if (residues_.find(resid) == residues_.end()) {
                Residue residue(resname, resid);
                residue.add_atom(frame.size() - 1);

                residues_.insert({resid, residue});
            } else {
                // Just add this atom to the residue
                residues_.at(resid).add_atom(frame.size() - 1);
            }
        }
    }

    std::string box = file_->readline();
    auto box_values = split(box, ' ');

    if (box_values.size() == 3) {
        auto a = parse<double>(box_values[0]) * 10;
        auto b = parse<double>(box_values[1]) * 10;
        auto c = parse<double>(box_values[2]) * 10;

        auto cell = UnitCell(a, b, c);
        frame.set_cell(cell);
    } else if (box_values.size() == 9) {
        auto v1_x = parse<double>(box_values[0]) * 10;
        auto v2_y = parse<double>(box_values[1]) * 10;
        auto v3_z = parse<double>(box_values[2]) * 10;

        assert(parse<double>(box_values[3]) == 0);
        assert(parse<double>(box_values[4]) == 0);

        auto v2_x = parse<double>(box_values[5]) * 10;

        assert(parse<double>(box_values[6]) == 0);

        auto v3_x = parse<double>(box_values[7]) * 10;
        auto v3_y = parse<double>(box_values[8]) * 10;

        auto H = Matrix3D(
            v1_x, v2_x, v3_x,
            0.00, v2_y, v3_y,
            0.00, 0.00, v3_z);

        auto cell = UnitCell(H);

        frame.set_cell(cell);
    }

    for (auto& residue: residues_) {
        frame.add_residue(residue.second);
    }
}

static std::string to_gro_index(uint64_t i) {
    if (i >= 99999) {
        warning("Too many atoms for GRO format, removing atomic id");
        return "*****";
    } else {
        return std::to_string(i + 1);
    }
}

void GROFormat::write(const Frame& frame) {
    fmt::print(*file_, "{}\n", frame.get<Property::STRING>("name").value_or("GRO File produced by chemfiles"));
    fmt::print(*file_, "{: >5d}\n", frame.size());

    // Only use numbers bigger than the biggest residue id as "resSeq" for
    // atoms without associated residue, and start generated residue id at
    // 1
    uint64_t max_resid = 1;
    for (const auto& residue: frame.topology().residues()) {
        auto resid = residue.id();
        if (resid && resid.value() > max_resid) {
            max_resid = resid.value() + 1;
        }
    }

    auto& positions = frame.positions();
    for (size_t i = 0; i < frame.size(); i++) {
        std::string resname = "XXXXX";
        std::string resid = "-1";
        auto residue = frame.topology().residue_for_atom(i);
        if (residue) {
            resname = residue->name();
            if (resname.length() > 5) {
                warning(
                    "Residue '{}' has a name too long for GRO format, it will be truncated.",
                    resname
                );
                resname = resname.substr(0, 5);
            }
        }

        if (residue && residue->id()) {
            auto value = residue->id().value();
            if (value <= 99999) {
                resid = std::to_string(value);
            } else {
                warning("Too many residues for GRO format, removing residue id");
            }
        } else {
            // We need to manually assign a residue ID
            auto value = max_resid++;
            if (value <= 99999) {
                resid = std::to_string(value);
            }
        }

        assert(resname.length() <= 5);
        auto pos = positions[i] / 10;
        check_values_size(pos, 8, "atomic position");

        if (frame.velocities()) {
            auto vel = (*frame.velocities())[i] / 10;
            check_values_size(vel, 8, "atomic velocity");
            fmt::print(
                *file_,
                "{: >5}{: <5}{: >5}{: >5}{:8.3f}{:8.3f}{:8.3f}{:8.4f}{:8.4f}{:8.4f}\n",
                resid, resname, frame[i].name(), to_gro_index(i), pos[0], pos[1], pos[2], vel[0], vel[1], vel[2]
            );
        } else {
            fmt::print(
                *file_,
                "{: >5}{: <5}{: >5}{: >5}{:8.3f}{:8.3f}{:8.3f}\n",
                resid, resname, frame[i].name(), to_gro_index(i), pos[0], pos[1], pos[2]
            );
        }
    }

    auto& cell = frame.cell();

    // While this line is free form, we should try to print it in a pretty way that most gro parsers expect
    // This means we cannot support incredibly large cell sizes, but these are likely not practical anyway
    if (cell.shape() == UnitCell::ORTHORHOMBIC || cell.shape() == UnitCell::INFINITE) {
        check_values_size(Vector3D(cell.a() / 10, cell.b() / 10, cell.c() / 10), 8, "Unit Cell");
        fmt::print(
            *file_,
            // Will print zeros if infinite, line is still required
            "  {:8.5f}  {:8.5f}  {:8.5f}\n",
            cell.a() / 10, cell.b() / 10, cell.c() / 10);
    } else { // Triclinic
        const auto& matrix = cell.matrix() / 10;
        check_values_size(Vector3D(matrix[0][0], matrix[1][1], matrix[2][2]), 8, "Unit Cell");
        check_values_size(Vector3D(matrix[0][1], matrix[0][2], matrix[1][2]), 8, "Unit Cell");
        fmt::print(
            *file_,
            "  {:8.5f}  {:8.5f}  {:8.5f} 0.0 0.0  {:8.5f} 0.0  {:8.5f}  {:8.5f}\n",
            matrix[0][0], matrix[1][1], matrix[2][2], matrix[0][1], matrix[0][2], matrix[1][2]
        );
    }

    steps_positions_.push_back(file_->tellg());
}

void check_values_size(const Vector3D& values, unsigned width, const std::string& context) {
    double max_pos = std::pow(10.0, width) - 1;
    double max_neg = -std::pow(10.0, width - 1) + 1;
    if (values[0] > max_pos || values[1] > max_pos || values[2] > max_pos ||
        values[0] < max_neg || values[1] < max_neg || values[2] < max_neg) {
        throw format_error(
            "value in {} is too big for representation in GRO format", context
        );
    }
}

bool forward(TextFile& file) {
    if (!file) {return false;}

    size_t natoms = 0;
    try {
        // Skip the comment line
        file.readline();
        natoms = parse<size_t>(file.readline());
    } catch (const FileError&) {
        // No more line left in the file
        return false;
    } catch (const Error&) {
        // We could not read an integer, so give up here
        return false;
    }

    try {
        file.readlines(natoms + 1);
    } catch (const FileError&) {
        // We could not read the lines from the file
        throw format_error(
            "not enough lines in '{}' for GRO format", file.path()
        );
    }
    return true;
}

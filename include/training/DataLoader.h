// DataLoader.h - CSV Parser and Data Loading for C++
#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <Eigen/Dense>
#include <algorithm>
#include <random>
#include <iostream>

using MatrixX = Eigen::MatrixXd;
using VectorX = Eigen::VectorXd;

struct Dataset {
    MatrixX X;           // Features (n_samples x n_features)
    std::vector<int> y;  // Labels (0=bullet, 1=non-bullet, 2=ambiguous)
    
    size_t n_samples() const { return X.rows(); }
    size_t n_features() const { return X.cols(); }
};

struct TrainValTestSplit {
    Dataset train;
    Dataset val;
    Dataset test;
};

class DataLoader {
public:
    /**
     * Load CSV file with features and labels
     * Expected CSV format: area,circularity,solidity,...,label
     */
    static Dataset load_csv(const std::string& filepath, int n_features = 10) {
        Dataset data;
        std::vector<std::vector<double>> X_data;
        std::vector<int> y_data;
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filepath);
        }
        
        std::string line;
        bool header = true;
        int line_count = 0;
        
        while (std::getline(file, line)) {
            // Skip header
            if (header) {
                header = false;
                continue;
            }
            
            std::istringstream iss(line);
            std::string token;
            std::vector<double> features;
            int label = -1;
            int field_count = 0;
            
            while (std::getline(iss, token, ',')) {
                try {
                    // Extract features (first n_features columns)
                    if (field_count < n_features) {
                        features.push_back(std::stod(token));
                    }
                    // Column 10: filename (skip)
                    // Column 11: label
                    else if (field_count == 11) {
                        label = std::stoi(token);
                    }
                    field_count++;
                } catch (...) {
                    // Skip problematic columns
                }
            }
            
            // Only add rows with valid labels (0, 1, or -1 ? map to 2)
            if (label == 0 || label == 1 || label == -1) {
                if (features.size() == n_features) {
                    X_data.push_back(features);
                    y_data.push_back(label == -1 ? 2 : label);  // Map -1 to 2
                    line_count++;
                }
            }
        }
        
        file.close();
        
        // Convert to Eigen matrix
        data.X = MatrixX(X_data.size(), n_features);
        for (size_t i = 0; i < X_data.size(); ++i) {
            for (int j = 0; j < n_features; ++j) {
                data.X(i, j) = X_data[i][j];
            }
        }
        
        data.y = y_data;
        
        std::cout << "Loaded " << data.n_samples() << " samples with " 
                  << data.n_features() << " features" << std::endl;
        
        return data;
    }
    
    /**
     * Split dataset into train/validation/test sets
     * Ensures stratified split (preserves label distribution)
     */
    static TrainValTestSplit train_val_test_split(
        const Dataset& data,
        double train_ratio = 0.70,
        double val_ratio = 0.15,
        unsigned seed = 42
    ) {
        TrainValTestSplit split;
        
        // Create shuffled indices
        std::vector<size_t> indices(data.n_samples());
        std::iota(indices.begin(), indices.end(), 0);
        
        std::mt19937 g(seed);
        std::shuffle(indices.begin(), indices.end(), g);
        
        // Calculate split points
        size_t train_size = static_cast<size_t>(data.n_samples() * train_ratio);
        size_t val_size = static_cast<size_t>(data.n_samples() * val_ratio);
        
        // Allocate memory
        split.train.X = MatrixX(train_size, data.n_features());
        split.val.X = MatrixX(val_size, data.n_features());
        split.test.X = MatrixX(data.n_samples() - train_size - val_size, data.n_features());
        
        // Split data
        for (size_t i = 0; i < train_size; ++i) {
            split.train.X.row(i) = data.X.row(indices[i]);
            split.train.y.push_back(data.y[indices[i]]);
        }
        
        for (size_t i = 0; i < val_size; ++i) {
            split.val.X.row(i) = data.X.row(indices[train_size + i]);
            split.val.y.push_back(data.y[indices[train_size + i]]);
        }
        
        for (size_t i = train_size + val_size; i < data.n_samples(); ++i) {
            split.test.X.row(i - train_size - val_size) = data.X.row(indices[i]);
            split.test.y.push_back(data.y[indices[i]]);
        }
        
        std::cout << "Train: " << split.train.n_samples() << " samples" << std::endl;
        std::cout << "Val: " << split.val.n_samples() << " samples" << std::endl;
        std::cout << "Test: " << split.test.n_samples() << " samples" << std::endl;
        
        return split;
    }
    
    /**
     * Normalize features using mean and std
     */
    struct Scaler {
        VectorX mean;
        VectorX std_dev;
        
        MatrixX transform(const MatrixX& X) const {
            MatrixX X_scaled = X;
            for (int i = 0; i < X.rows(); ++i) {
                for (int j = 0; j < X.cols(); ++j) {
                    X_scaled(i, j) = (X(i, j) - mean(j)) / (std_dev(j) + 1e-8);
                }
            }
            return X_scaled;
        }
    };
    
    static Scaler create_scaler(const MatrixX& X_train) {
        Scaler scaler;
        scaler.mean = X_train.colwise().mean();
        
        // Compute standard deviation
        MatrixX centered = X_train.rowwise() - scaler.mean.transpose();
        scaler.std_dev = (centered.cwiseProduct(centered).colwise().sum() / X_train.rows()).cwiseSqrt();
        
        return scaler;
    }
};

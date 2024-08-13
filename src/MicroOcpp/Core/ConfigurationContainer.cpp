// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/ConfigurationContainer.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

ConfigurationContainer::~ConfigurationContainer() {

}

ConfigurationContainerVolatile::ConfigurationContainerVolatile(const char *filename, bool accessible) :
        ConfigurationContainer(filename, accessible), MemoryManaged("v16.Configuration.ContainerVoltaile.", filename), configurations(makeVector<std::shared_ptr<Configuration>>(getMemoryTag())) {

}

bool ConfigurationContainerVolatile::load() {
    return true;
}

bool ConfigurationContainerVolatile::save() {
    return true;
}

std::shared_ptr<Configuration> ConfigurationContainerVolatile::createConfiguration(TConfig type, const char *key) {
    auto res = std::shared_ptr<Configuration>(makeConfiguration(type, key).release(), std::default_delete<Configuration>(), makeAllocator<Configuration>("v16.Configuration.", key));
    if (!res) {
        //allocation failure - OOM
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    configurations.push_back(res);
    return res;
}

void ConfigurationContainerVolatile::remove(Configuration *config) {
    for (auto entry = configurations.begin(); entry != configurations.end();) {
        if (entry->get() == config) {
            entry = configurations.erase(entry);
        } else {
            entry++;
        }
    }
}

size_t ConfigurationContainerVolatile::size() {
    return configurations.size();
}

Configuration *ConfigurationContainerVolatile::getConfiguration(size_t i) {
    return configurations[i].get();
}

std::shared_ptr<Configuration> ConfigurationContainerVolatile::getConfiguration(const char *key) {
    for (auto& entry : configurations) {
        if (entry->getKey() && !strcmp(entry->getKey(), key)) {
            return entry;
        }
    }
    return nullptr;
}

void ConfigurationContainerVolatile::add(std::shared_ptr<Configuration> c) {
    configurations.push_back(std::move(c));
}

namespace MicroOcpp {

std::unique_ptr<ConfigurationContainerVolatile> makeConfigurationContainerVolatile(const char *filename, bool accessible) {
    return std::unique_ptr<ConfigurationContainerVolatile>(new ConfigurationContainerVolatile(filename, accessible));
}

} //end namespace MicroOcpp

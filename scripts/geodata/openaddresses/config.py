import os
import six
import yaml

this_dir = os.path.realpath(os.path.dirname(__file__))

OPENADDRESSES_PARSER_DATA_CONFIG = os.path.join(this_dir, os.pardir, os.pardir, os.pardir,
                                                'resources', 'parser', 'data_sets', 'openaddresses.yaml')


class OpenAddressesConfig(object):
    def __init__(self, path=OPENADDRESSES_PARSER_DATA_CONFIG):
        self.path = path

        config = yaml.load(open(path))
        self.config = config['global']
        self.country_configs = config['countries']

    @property
    def sources(self):
        for country, config in six.iteritems(self.country_configs):
            for file_config in config.get('files', []):
                filename = file_config['filename'].rsplit('.', 1)[0]

                yield country, filename

            for subdir, subdir_config in six.iteritems(config.get('subdirs', {})):
                for file_config in subdir_config.get('files', []):
                    filename = file_config['filename'].rsplit('.', 1)[0]

                    yield country, subdir, filename

openaddresses_config = OpenAddressesConfig()
